#!/usr/bin/env python3
"""Build and inspect PS2 development ELFs; never package game ROMs.

The default target exercises the existing reference rasterizer and PS2 host.
It does not contain the statically translated game. A core-probe manifest can
compile the actual generated game and runtime for memory/portability analysis.
Neither target is advertised as a playable release.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import sys
import threading
import time


ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
LOCK_PATH = HERE / "toolchain.lock.json"
LOCK = json.loads(LOCK_PATH.read_text())
PREFIX = LOCK["compiler_prefix"]
BRINGUP_SOURCES = [
    "platform/ps2/src/main.cpp",
    "platform/ps2/src/gs_display.cpp",
    "platform/ps2/src/pad_input.cpp",
    "platform/ps2/src/bringup_scene.cpp",
    "platform/ps2/src/presentation.cpp",
    "platform/ps2/src/controls.cpp",
    "platform/ps2/src/status_screen.cpp",
    "src/runtime/raster.cpp",
]
BRINGUP_LIBRARIES = ["draw", "graph", "dma", "packet", "patches", "pad", "m"]
LIMITATIONS = [
    "Development ELF, not a playable Sega Rally release.",
    "Compilation and ELF inspection do not establish PS2 hardware execution.",
    "Arcade timing, game rendering, audio and full-race fidelity remain unverified on PS2.",
]


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def project_path(value: str) -> Path:
    path = Path(value)
    return (ROOT / path).resolve() if not path.is_absolute() else path.resolve()


def relative(path: Path) -> str:
    return str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)


def project_headers(sources: list[Path], includes: list[Path]) -> list[Path]:
    """Follow project include dependencies without scanning ignored worktrees.

    SDK and standard-library headers are supplied by the pinned toolchain.
    Conditional includes that do not exist in this checkout are left for the
    compiler to resolve; inactive platform branches need not be installed.
    """
    pending = list(sources)
    visited, headers = set(), set()
    pattern = re.compile(r'^\s*#\s*include\s*(["<])([^">\n]+)[">]', re.MULTILINE)
    while pending:
        path = pending.pop().resolve()
        if path in visited:
            continue
        visited.add(path)
        for opening, name in pattern.findall(path.read_text()):
            roots = [path.parent, *includes] if opening == '"' else includes
            candidate = next((root / name for root in roots if (root / name).is_file()), None)
            if candidate is not None:
                candidate = candidate.resolve()
                headers.add(candidate)
                pending.append(candidate)
    return sorted(headers - {path.resolve() for path in sources})


def compilation_key(source: Path, headers: list[Path], fingerprints: dict[str, str],
                    context: str) -> str:
    """Keep unrelated frontend header edits out of generated-core cache keys."""
    inputs = {relative(path): fingerprints[relative(path)] for path in [source, *headers]}
    return hashlib.sha256(json.dumps({"context": context, "inputs": inputs},
                                     sort_keys=True).encode()).hexdigest()


def inspect_elf(path: Path) -> dict:
    """Reject host binaries and ELF segments outside the EE's 32 MiB RAM."""
    data = path.read_bytes()
    if len(data) < 52 or data[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Expected a 32-bit little-endian ELF, not a host executable")
    fields = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    kind, machine, version, entry, phoff, _, flags, _, phsize, phnum, *_ = fields
    if kind != 2 or machine != 8 or version != 1 or phsize < 32:
        raise ValueError("Expected an executable MIPS ELF with valid program headers")
    segments = []
    for index in range(phnum):
        offset = phoff + index * phsize
        if offset + 32 > len(data):
            raise ValueError("Truncated ELF program headers")
        ptype, fileoff, address, physical, filesz, memsz, protection, align = struct.unpack_from(
            "<IIIIIIII", data, offset
        )
        if ptype != 1:
            continue
        if filesz > memsz or fileoff + filesz > len(data):
            raise ValueError("Invalid ELF load segment size")
        if address < 0x100000 or address + memsz > 0x2000000:
            raise ValueError("ELF load segment exceeds available EE user RAM")
        segments.append({"address": address, "physical_address": physical,
                         "file_bytes": filesz, "memory_bytes": memsz,
                         "flags": protection, "alignment": align})
    if not segments or not any(s["address"] <= entry < s["address"] + s["memory_bytes"]
                               and s["flags"] & 1 for s in segments):
        raise ValueError("ELF entry point is outside executable load segments")
    high_water = max(s["address"] + s["memory_bytes"] for s in segments)
    return {"path": relative(path), "sha256": digest(path), "file_bytes": len(data),
            "format": "ELF32 little-endian MIPS executable", "entry": entry,
            "elf_flags": flags, "load_segments": segments,
            "load_segment_memory_bytes": sum(s["memory_bytes"] for s in segments),
            "ram_high_water_bytes": high_water,
            "remaining_before_stack_heap_bytes": 0x2000000 - high_water,
            "runtime_allocations_included": False}


class Runner:
    def __init__(self, args: argparse.Namespace):
        self.args = args
        self.output = args.output
        self.output.mkdir(parents=True, exist_ok=True)
        self.commands = []
        self.guard = threading.Lock()
        machine = platform.machine().lower()
        self.container_platform = args.container_platform or (
            "linux/arm64" if machine in ("arm64", "aarch64") else "linux/amd64")
        self.image = LOCK["platforms"][self.container_platform]["image"]
        self.ps2dev = "/usr/local/ps2dev" if args.backend == "docker" else str(args.ps2dev)
        self.sdk = self.ps2dev + "/ps2sdk"

    def path(self, value: Path) -> str:
        value = value.resolve()
        if self.args.backend != "docker":
            return str(value)
        if value == self.output:
            return "/out"
        if value == ROOT:
            return "/workspace"
        if value.is_relative_to(self.output):
            return "/out/" + str(value.relative_to(self.output))
        if value.is_relative_to(ROOT):
            return "/workspace/" + str(value.relative_to(ROOT))
        raise ValueError("Container inputs must be inside the repository: " + str(value))

    def tool(self, name: str) -> str:
        return self.ps2dev + "/ee/bin/" + PREFIX + name

    def run(self, command: list[str], name: str, container: bool = True) -> str:
        actual = command
        if self.args.backend == "docker" and container:
            actual = ["docker", "run", "--rm", "--pull=never", "--network=none",
                      "--read-only", "--tmpfs", "/tmp", "--platform", self.container_platform,
                      "--mount", f"type=bind,source={ROOT},target=/workspace,readonly",
                      "--mount", f"type=bind,source={self.output},target=/out",
                      "--workdir", "/out", self.image, *command]
        started = time.monotonic()
        result = subprocess.run(actual, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, cwd=ROOT)
        record = {"name": name, "command": actual, "returncode": result.returncode,
                  "elapsed_seconds": round(time.monotonic() - started, 3)}
        with self.guard:
            self.commands.append(record)
            (self.output / (name + ".log")).write_text(result.stdout)
        if result.returncode:
            raise RuntimeError(f"{name} failed ({result.returncode}):\n{result.stdout[-8000:]}")
        return result.stdout

    def identity(self) -> dict:
        if self.args.backend == "docker":
            if not shutil.which("docker"):
                raise RuntimeError("Docker is unavailable; use --backend native with PS2DEV installed")
            try:
                info = json.loads(self.run(["docker", "inspect", "--type", "image", self.image],
                                          "container-inspect", container=False))[0]
            except RuntimeError as error:
                raise RuntimeError("Cannot inspect the pinned Docker image; check daemon access first. "
                                   "If the image is missing, install explicitly with:\n"
                                   f"docker pull --platform {self.container_platform} {self.image}\n"
                                   + str(error)) from error
            identity = {"backend": "docker", "image": self.image,
                        "container_platform": self.container_platform,
                        "image_id": info["Id"], "installed_bytes": info["Size"]}
        else:
            identity = {"backend": "native", "ps2dev": self.ps2dev,
                        "pinned_image_used": False}
        identity["compiler_version"] = self.run([self.tool("g++"), "--version"], "compiler-version").strip()
        identity["compiler_target"] = self.run([self.tool("g++"), "-dumpmachine"], "compiler-target").strip()
        if identity["compiler_target"] != "mips64r5900el-ps2-elf":
            raise RuntimeError("The selected compiler does not target the PS2 Emotion Engine")
        return identity


def build_spec(args: argparse.Namespace) -> dict:
    if args.target == "bringup":
        return {"sources": BRINGUP_SOURCES, "includes": ["platform/ps2/include", "src"],
                "libraries": BRINGUP_LIBRARIES, "defines": [], "compile_flags": [],
                "embed_iop_modules": True}
    if not args.core_manifest:
        raise ValueError("core-probe requires --core-manifest with the actual game/runtime source list")
    spec = json.loads(args.core_manifest.read_text())
    if not isinstance(spec.get("sources"), list) or not spec["sources"]:
        raise ValueError("Core manifest requires a nonempty sources array")
    for key in ("includes", "defines", "compile_flags", "libraries"):
        spec.setdefault(key, [])
    spec.setdefault("embed_iop_modules", False)
    if "iop_modules" in spec and (not isinstance(spec["iop_modules"], list) or
                                 any(not isinstance(value, str) or not value.replace("_", "").isalnum()
                                     for value in spec["iop_modules"])):
        raise ValueError("iop_modules must contain SDK module names without paths or extensions")
    if any(value == "-Ofast" or "fast-math" in value or "unsafe-math" in value
           for value in spec["compile_flags"]):
        raise ValueError("Fast-math is not permitted for the shared game/runtime")
    return spec


def build(args: argparse.Namespace, report: dict) -> None:
    runner = Runner(args)
    report["toolchain"] = runner.identity()
    spec = build_spec(args)
    sources = [project_path(value) for value in spec["sources"]]
    includes = [project_path(value) for value in spec["includes"]]
    for source in sources:
        if not source.is_file():
            raise ValueError("Missing build source: " + str(source))
    (args.output / "obj").mkdir(exist_ok=True)
    # A frontend header edit must not rebuild every generated i960 chunk.
    dependencies = {source: project_headers([source], includes) for source in sources}
    headers = sorted({header for values in dependencies.values() for header in values})
    fingerprints = {relative(p): digest(p) for p in sorted(set(sources + headers))}
    report["source_sha256"] = fingerprints
    report["compile_dependencies"] = {relative(source): [relative(p) for p in headers]
                                      for source, headers in dependencies.items()}
    common = ["-D_EE", "-G0", "-" + args.optimization, "-Wall",
              "-ffunction-sections", "-fdata-sections", "-fno-fast-math", "-ffp-contract=off",
              "-I" + runner.sdk + "/ee/include", "-I" + runner.sdk + "/common/include",
              "-ffile-prefix-map=" + runner.path(ROOT) + "=/source"]
    common += ["-I" + runner.path(p) for p in includes]
    common += ["-D" + value for value in spec["defines"]] + spec["compile_flags"]
    common += ["-fno-fast-math", "-ffp-contract=off"]
    common_signature = hashlib.sha256(json.dumps(
        {"toolchain": report["toolchain"], "flags": common},
        sort_keys=True).encode()).hexdigest()

    def compile_source(source: Path) -> Path:
        name = source.stem + "-" + hashlib.sha256(relative(source).encode()).hexdigest()[:12]
        signature = compilation_key(source, dependencies[source], fingerprints, common_signature)
        obj = args.output / "obj" / (name + ".o")
        stamp = obj.with_suffix(".json")
        if obj.exists() and stamp.exists():
            cached = json.loads(stamp.read_text())
            if cached.get("signature") == signature and cached.get("sha256") == digest(obj):
                return obj
        cpp = source.suffix not in (".c", ".S", ".s")
        standard = ["-std=c++20"] if cpp else (["-std=gnu11"] if source.suffix == ".c" else [])
        runner.run([runner.tool("g++" if cpp else "gcc"), *standard, *common,
                    "-c", runner.path(source), "-o", runner.path(obj)], "compile-" + name)
        write_json(stamp, {"schema": 2, "signature": signature, "sha256": digest(obj)})
        return obj

    try:
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            objects = list(pool.map(compile_source, sources))
        modules = spec.get("iop_modules", ["sio2man", "padman"] if spec["embed_iop_modules"] else [])
        if modules:
            for module in modules:
                obj = args.output / "obj" / (module + "_irx.o")
                generated = obj.with_suffix(".c")
                runner.run([runner.sdk + "/bin/bin2c", runner.sdk + "/iop/irx/" + module + ".irx",
                            runner.path(generated), module + "_irx"], "embed-" + module)
                runner.run([runner.tool("gcc"), "-D_EE", "-G0", "-O2", "-c",
                            runner.path(generated), "-o", runner.path(obj)], "compile-" + module)
                objects.append(obj)
        elf = args.output / ("segarally-ps2-" + args.target + ".elf")
        link = [runner.tool("g++"), "-G0", "-" + args.optimization,
                "-T" + runner.sdk + "/ee/startup/linkfile", "-L" + runner.sdk + "/ee/lib",
                "-Wl,-zmax-page-size=128", "-Wl,--gc-sections",
                "-Wl,-Map," + runner.path(elf.with_suffix(".map")), "-o", runner.path(elf)]
        link += [runner.path(p) for p in objects]
        link += ["-Wl,--start-group", *["-l" + value for value in spec["libraries"]], "-Wl,--end-group"]
        runner.run(link, "link")
        report["elf"] = inspect_elf(elf)
        report["size"] = runner.run([runner.tool("size"), runner.path(elf)], "size").strip()
        report["status"] = "cross_compiled"
        print(f"Built {relative(elf)} ({report['elf']['file_bytes']:,} bytes); hardware execution unverified.")
    finally:
        report["commands"] = runner.commands


def smoke(args: argparse.Namespace, report: dict) -> None:
    # The scene assertions run on the development host, not a PS2 emulator.
    sources = [HERE / "src/host_smoke.cpp", HERE / "src/bringup_scene.cpp",
               HERE / "src/presentation.cpp", HERE / "src/controls.cpp",
               ROOT / "src/runtime/raster.cpp"]
    compiler = shutil.which(args.host_cxx)
    if not compiler:
        raise RuntimeError("Host C++ compiler not found: " + args.host_cxx)
    missing = [str(p) for p in sources if not p.is_file()]
    if missing:
        raise RuntimeError("Missing host smoke source: " + ", ".join(missing))
    binary = args.output / "ps2-bringup-host-tests"
    command = [compiler, "-std=c++20", "-O2", "-fno-fast-math", "-ffp-contract=off",
               "-I" + str(HERE / "include"), "-I" + str(ROOT / "src"),
               *[str(p) for p in sources], "-o", str(binary)]
    result = subprocess.run(command, text=True, capture_output=True)
    (args.output / "host-compile.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    tested = subprocess.run([str(binary)], text=True, capture_output=True)
    (args.output / "host-test.log").write_text(tested.stdout + tested.stderr)
    report.update(status="host_tests_passed" if tested.returncode == 0 else "host_tests_failed",
                  host_test_returncode=tested.returncode, ps2_hardware_tested=False,
                  source_sha256={relative(p): digest(p) for p in sources})
    if tested.returncode:
        raise RuntimeError(tested.stdout + tested.stderr)
    print(tested.stdout.strip() or "Portable PS2 presentation tests passed on the host.")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("build", "smoke", "report"))
    parser.add_argument("--target", choices=("bringup", "core-probe"), default="bringup")
    parser.add_argument("--backend", choices=("docker", "native"), default="docker")
    parser.add_argument("--output", type=project_path, default=ROOT / "build/ps2-bringup")
    parser.add_argument("--ps2dev", type=Path, default=Path(os.environ.get("PS2DEV", "/usr/local/ps2dev")))
    parser.add_argument("--container-platform", choices=tuple(LOCK["platforms"]))
    parser.add_argument("--core-manifest", type=project_path)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--optimization", choices=("O2", "O3", "Os"), default="O2")
    parser.add_argument("--host-cxx", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args(argv)
    # argparse only applies the type converter to string defaults. Resolve the
    # Path default too, so a symlink cannot redirect output outside build/.
    args.output = args.output.resolve()
    if not args.output.is_relative_to(ROOT / "build"):
        parser.error("--output must be below the repository's ignored build directory")
    if args.jobs < 1 or args.jobs > 32:
        parser.error("--jobs must be between 1 and 32")
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"schema": 1, "target": args.target, "mode": args.mode,
              "status": "started", "playable": False, "ps2_hardware_tested": False,
              "roms_packaged": False, "limitations": LIMITATIONS,
              "toolchain_lock_sha256": digest(LOCK_PATH)}
    try:
        if args.mode == "build":
            build(args, report)
        elif args.mode == "smoke":
            smoke(args, report)
        else:
            runner = Runner(args)
            report["toolchain"] = runner.identity()
            elf = args.output / ("segarally-ps2-" + args.target + ".elf")
            report["elf"] = inspect_elf(elf) if elf.exists() else None
            report["status"] = "inspected" if elf.exists() else "toolchain_ready_no_elf"
            print(json.dumps(report, indent=2))
        return 0
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        report.update(status="failed", error=str(error))
        print(str(error), file=sys.stderr)
        return 1
    finally:
        write_json(args.output / (args.mode + "-" + args.target + ".json"), report)


if __name__ == "__main__":
    sys.exit(main())
