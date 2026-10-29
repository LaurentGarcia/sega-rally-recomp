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
    "platform/ps2/src/model2_gs.cpp",
    "platform/ps2/src/vu0_pack.cpp",
    "platform/ps2/src/pad_input.cpp",
    "platform/ps2/src/bringup_scene.cpp",
    "platform/ps2/src/presentation.cpp",
    "platform/ps2/src/controls.cpp",
    "platform/ps2/src/status_screen.cpp",
    "src/runtime/raster.cpp",
]
BRINGUP_LIBRARIES = ["draw", "graph", "dma", "packet", "patches", "pad", "m"]
ENGINE_SOURCES = (
    "platform/ps2/src/native_display.cpp",
    "platform/ps2/src/native_frame.cpp",
    "platform/ps2/src/native_geometry.cpp",
    "platform/ps2/src/native_geometry_float.cpp",
    "platform/ps2/src/pad_input.cpp", "platform/ps2/src/storage.cpp",
    "platform/ps2/src/audio_output.cpp", "platform/ps2/src/vu0_pack.cpp",
    "platform/ps2/src/world_scene.cpp",
    "platform/ps2/src/vu1_transform.cpp", "platform/ps2/src/vu1_transform_math.cpp",
    "platform/ps2/src/vu1_transform.vsm",
)
STUDY_SOURCES = ("platform/ps2/src/native_mesh.cpp", "platform/ps2/src/native_scene.cpp")
GAME_SOURCES = ("platform/ps2/native/race_progress.cpp", "platform/ps2/native/car_pose.cpp",
                "platform/ps2/native/angle_fold.cpp", "platform/ps2/native/orientation.cpp",
                "platform/ps2/native/direction_blend.cpp", "platform/ps2/native/contact_neighborhood.cpp",
                "platform/ps2/native/alternate_orientation.cpp", "platform/ps2/native/contact_projection.cpp",
                "platform/ps2/native/cabinet_steering.cpp",
                "platform/ps2/native/cabinet_steering_filter.cpp",
                "platform/ps2/native/contact_search.cpp",
                "platform/ps2/native/main_car_pose.cpp",
                "platform/ps2/native/rounded_cross_product.cpp",
                "platform/ps2/native/rounded_affine_transform.cpp",
                "platform/ps2/native/contact_result.cpp",
                "platform/ps2/native/contact_normalization.cpp",
                "platform/ps2/native/recovered_scalar_metric.cpp",
                "platform/ps2/native/contact_aggregate.cpp",
                "platform/ps2/native/contact_response.cpp",
                "platform/ps2/native/contact_pass_response.cpp",
                "platform/ps2/native/contact_point_preparation.cpp",
                "platform/ps2/native/candidate_contact_pass.cpp",
                "platform/ps2/native/main_candidate.cpp",
                "platform/ps2/native/main_contact_slot.cpp",
                "platform/ps2/native/paired_balance.cpp",
                "platform/ps2/native/vehicle_parameter_selection.cpp",
                "platform/ps2/native/main_contact_step.cpp",
                "platform/ps2/native/main_balance_geometry.cpp",
                "platform/ps2/native/segment_selection.cpp",
                "platform/ps2/native/vehicle_arithmetic.cpp",
                "platform/ps2/native/recovered_atangent.cpp",
                "platform/ps2/native/driving_assets.cpp",
                "platform/ps2/native/main_balance_math.cpp",
                "platform/ps2/native/main_balance_update.cpp",
                "platform/ps2/native/scene_contact_damping.cpp",
                "platform/ps2/native/main_motion_update.cpp",
                "platform/ps2/native/main_update_entry.cpp",
                "platform/ps2/native/main_front_update.cpp",
                "platform/ps2/native/main_vehicle_update.cpp",
                "platform/ps2/native/main_vehicle_checkpoint.cpp",
                "platform/ps2/native/rally_body_scene.cpp",
                "platform/ps2/native/native_vehicle_demo.cpp")
# Preserve the existing native-port object order while the demo omits study code.
NATIVE_TOOLKIT_SOURCES = (*ENGINE_SOURCES[:3], *STUDY_SOURCES, *ENGINE_SOURCES[3:])
NATIVE_SOURCES = ["platform/ps2/native/main.cpp", *GAME_SOURCES, *NATIVE_TOOLKIT_SOURCES]
ENGINE_DEMO_SOURCES = ["platform/ps2/samples/engine_demo/main.cpp", *ENGINE_SOURCES]
ENGINE_HEADERS = {
    "audio_output.h", "pcm_protocol.h", "pcm_resampler.h", "pcm_ring.h",
    "native_display.h", "native_geometry.h", "native_geometry_float.h",
    "native_geometry_float_internal.h", "world_scene.h",
    "pad_input.h", "pad_state.h", "debug_controls.h", "storage.h", "vu0_pack.h", "vu1_transform.h", "diagnostic_font.h",
}
STUDY_HEADERS = {"native_mesh.h", "native_scene.h"}
NATIVE_TOOLKIT_HEADERS = ENGINE_HEADERS | STUDY_HEADERS
NATIVE_IOP_SOURCES = ("platform/ps2/iop/pcm_server.c", "platform/ps2/iop/imports.c")
NATIVE_TARGETS = ("native-port", "engine-demo")
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


def native_source_path(value: str | Path) -> Path:
    """Preserve the declared identity before resolving a reviewed native file.

    Merely classifying the resolved destination would let an engine source
    become an otherwise allowed game source in the native-port target.
    """
    original = ROOT / value
    declared = Path(os.path.abspath(original))
    # Resolve the original spelling: collapsing symlink/.. first can identify
    # a different file than the compiler's actual include lookup.
    resolved = original.resolve()
    if resolved != declared:
        raise ValueError("Native PS2 redirected reviewed path is forbidden: " + relative(declared) + " -> " + relative(resolved))
    return declared


def project_headers(sources: list[Path], includes: list[Path], *,
                    reject_redirected: bool = False) -> list[Path]:
    """Follow project include dependencies without scanning ignored worktrees.

    SDK and standard-library headers are supplied by the pinned toolchain.
    Conditional includes that do not exist in this checkout are left for the
    compiler to resolve; inactive platform branches need not be installed.
    """
    pending = [(source, [source.parent, *includes] if source.suffix == ".vsm" else None) for source in sources]
    visited, headers = set(), set()
    pattern = re.compile(r'^\s*#\s*include\s*(["<])([^">\n]+)[">]', re.MULTILINE)
    while pending:
        declared, assembly_roots = pending.pop()
        assembly = assembly_roots is not None
        path = native_source_path(declared) if reject_redirected else declared.resolve()
        identity = (path, tuple(assembly_roots) if assembly else None)
        if identity in visited:
            continue
        visited.add(identity)
        text = path.read_text()
        if assembly:
            # DVP uses assembler includes, not the C preprocessor. Resolve all
            # textual inputs explicitly; never hide binary payloads in objects.
            text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
            text = re.sub(r'[;#].*', '', text)
            directives = re.findall(r'(?im)^\s*(?:[\w.$]+:\s*)?\.(include|incbin)\b([^\n]*)', text)
            references = []
            for directive, argument in directives:
                match = re.fullmatch(r'\s*"([^"\\\n]+)"\s*', argument)
                if directive.lower() != "include" or not match:
                    raise ValueError("DVP assembly forbids binary or opaque includes: " + relative(path))
                references.append(('"', match.group(1)))
        else:
            references = pattern.findall(text)
        for opening, name in references:
            roots = assembly_roots if assembly else ([path.parent, *includes] if opening == '"' else includes)
            candidate = next((root / name for root in roots if (root / name).is_file()), None)
            if candidate is None and assembly:
                raise ValueError("Missing DVP assembly include: " + name)
            if candidate is not None:
                candidate = native_source_path(candidate) if reject_redirected else candidate.resolve()
                headers.add(candidate)
                pending.append((candidate, assembly_roots))
    return sorted(headers - {path.resolve() for path in sources})


def compilation_key(source: Path, headers: list[Path], fingerprints: dict[str, str],
                    context: str) -> str:
    """Keep unrelated frontend header edits out of generated-core cache keys."""
    inputs = {relative(path): fingerprints[relative(path)] for path in [source, *headers]}
    return hashlib.sha256(json.dumps({"context": context, "inputs": inputs},
                                     sort_keys=True).encode()).hexdigest()


def verify_source_snapshot(fingerprints: dict[str, str]) -> None:
    changed = [name for name, expected in fingerprints.items()
               if not (ROOT / name).is_file() or digest(ROOT / name) != expected]
    if changed:
        raise RuntimeError("Build inputs changed during compilation; rerun incrementally: " + ", ".join(sorted(changed)))


def native_roles() -> dict[str, str]:
    """Reviewed files only; a new file cannot silently inherit directory trust."""
    roles = {name: "engine" for name in (*ENGINE_SOURCES, *NATIVE_IOP_SOURCES)}
    roles.update({"platform/ps2/include/" + name: "engine" for name in ENGINE_HEADERS})
    roles.update({name: "study" for name in STUDY_SOURCES})
    roles.update({"platform/ps2/include/" + name: "study" for name in STUDY_HEADERS})
    roles.update({name: "game" for name in GAME_SOURCES})
    roles.update({str(Path(name).with_suffix(".h")): "game" for name in GAME_SOURCES})
    roles["platform/ps2/native/main_vehicle_checkpoint_fields.h"] = "game"
    roles["platform/ps2/native/main.cpp"] = "native_app"
    roles["platform/ps2/samples/engine_demo/main.cpp"] = "demo_app"
    return roles


def engine_header_paths() -> list[Path]:
    # Include unused public headers in both applications' boundary and identity.
    return [ROOT / "platform/ps2/include" / name for name in sorted(ENGINE_HEADERS)]


def validate_native_boundary(paths: list[Path], target: str = "native-port",
                             includes: list[Path] | None = None) -> dict:
    """Only original native game code and explicitly reviewed PS2 services.

    Check resolved paths, including transitive project headers and IOP files;
    a toolkit symlink cannot import the archived machine or generated code.
    """
    if target not in NATIVE_TARGETS:
        raise ValueError("Unknown native target: " + target)
    includes = includes if includes is not None else [ROOT / "platform/ps2/include", ROOT / "platform/ps2/native"]
    # Inspect each header's own closure as well as every translation unit. This
    # prevents an application from laundering an engine->game include through
    # its otherwise permitted aggregate dependency set or an unused source.
    paths = sorted({native_source_path(path) for path in paths})
    paths = sorted(set(paths + project_headers([path for path in paths if path.is_file()], includes,
                                               reject_redirected=True)))
    roles = native_roles()
    permitted = {"engine", "demo_app"} if target == "engine-demo" else {"engine", "study", "game", "native_app"}
    rejected = []
    for path in paths:
        name = relative(path.resolve())
        if roles.get(name) not in permitted:
            rejected.append(name)
        elif path.is_file():
            # The dependency walk resolves literal includes. Macro-selected
            # includes would make its closed source inventory incomplete.
            for directive, argument in re.findall(r'^\s*#\s*(include_next|include|import)\b([^\n]*)', path.read_text(), re.MULTILINE):
                if directive != "include" or not re.match(r'\s*[<"][^">\n]+[">]', argument):
                    raise ValueError("Native PS2 target forbids opaque include directives: " + name)
    if rejected:
        raise ValueError("Native PS2 target forbids reference-machine/generated dependencies: " + ", ".join(sorted(set(rejected))))
    allowed = {"engine": {"engine"}, "study": {"study", "engine"}, "game": {"game", "engine"},
               "native_app": {"native_app", "game", "study", "engine"}, "demo_app": {"demo_app", "engine"}}
    for source in paths:
        source_name = relative(source)
        role = roles[source_name]
        if not source.is_file():
            continue
        for header in project_headers([source], includes, reject_redirected=True):
            name = relative(header)
            if roles[name] not in allowed[role]:
                raise ValueError(f"Native PS2 dependency direction forbidden: {source_name} ({role}) -> {name} ({roles[name]})")
    return {"execution_model": "original_ps2_engine_demo" if target == "engine-demo" else "native_ps2_rewrite_bootstrap",
            "translated_processors_linked": False, "arcade_machine_runtime_linked": False,
            "checked_project_files": [relative(path) for path in paths],
            "project_file_roles": {relative(path): roles[relative(path)] for path in paths},
            "dependency_directions_checked": True,
            "limitations": ["Application bootstraps may call the external PS2SDK directly; no lifecycle facade is enforced.",
                            "This is a project-source dependency check, not a sandbox or proof of general program behavior."]}


def shared_engine_manifest(fingerprints: dict[str, str]) -> dict:
    roles = native_roles()
    sources = {name: value for name, value in sorted(fingerprints.items()) if roles.get(name) == "engine"}
    return {"schema": 1, "source_sha256": sources,
            "manifest_sha256": hashlib.sha256(json.dumps(sources, sort_keys=True, separators=(",", ":")).encode()).hexdigest(),
            "scope": "Shared engine source identity; does not imply identical compiler flags, objects or runtime behavior"}


def elf_name(target: str) -> str:
    return "ps2-engine-demo.elf" if target == "engine-demo" else "segarally-ps2-" + target + ".elf"


def validate_native_symbols(symbols: str) -> None:
    forbidden = re.compile(r'\b(?:rt|snd|gen|sndgen|tgpgen)::|\bapp::Model2Frame\b')
    matches = [line.strip() for line in symbols.splitlines() if forbidden.search(line)]
    if matches:
        raise ValueError("Native PS2 ELF defines forbidden reference-machine symbols: " + "; ".join(matches[:8]))


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

    def dvp_tool(self, name: str) -> str:
        return self.ps2dev + "/dvp/bin/dvp-" + name

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
    if args.target in NATIVE_TARGETS:
        if args.core_manifest:
            raise ValueError(args.target + " uses a fixed original-source inventory; --core-manifest is forbidden")
        demo = args.target == "engine-demo"
        return {"sources": ENGINE_DEMO_SOURCES if demo else NATIVE_SOURCES,
                "includes": ["platform/ps2/include"] if demo else ["platform/ps2/native", "platform/ps2/include"],
                "libraries": ["draw", "graph", "dma", "packet", "patches", "pad", "fileXio", "m"],
                "defines": ["PS2_ENGINE_DEMO=1"] if demo else ["RALLY_NATIVE_PS2=1"], "compile_flags": [], "embed_iop_modules": True,
                "iop_modules": ["sio2man", "padman", "iomanX", "fileXio", "bdm", "bdmfs_fatfs", "usbd", "usbmass_bd", "libsd", "rally_pcm"],
                "pcm_transport": True}
    if args.target == "audio-fixture":
        return {"sources": ["platform/ps2/tests/native_audio_fixture.cpp", "platform/ps2/src/audio_output.cpp"],
                "includes": ["platform/ps2/include"], "libraries": ["patches", "m"],
                "defines": [], "compile_flags": [], "embed_iop_modules": True,
                "iop_modules": ["libsd", "rally_pcm"], "pcm_transport": True}
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
    spec = build_spec(args)
    source_path = native_source_path if args.target in NATIVE_TARGETS else project_path
    sources = [source_path(value) for value in spec["sources"]]
    runner = Runner(args)
    report["toolchain"] = runner.identity()
    includes = [project_path(value) for value in spec["includes"]]
    for source in sources:
        if not source.is_file():
            raise ValueError("Missing build source: " + str(source))
    if any(source.suffix == ".vsm" for source in sources):
        report["toolchain"]["dvp_assembler_version"] = runner.run(
            [runner.dvp_tool("as"), "--version"], "dvp-assembler-version").strip()
    (args.output / "obj").mkdir(exist_ok=True)
    # A frontend header edit must not rebuild every generated i960 chunk.
    dependencies = {source: project_headers([source], includes) for source in sources}
    headers = sorted({header for values in dependencies.values() for header in values})
    if args.target in NATIVE_TARGETS:
        headers = sorted(set(headers + engine_header_paths()))
    fingerprints = {relative(p): digest(p) for p in sorted(set(sources + headers))}
    if spec.get("pcm_transport"):
        iop_sources = [HERE / "iop/pcm_server.c", HERE / "iop/imports.c"]
        iop_headers = project_headers(iop_sources, [HERE / "include"])
        fingerprints.update({relative(p): digest(p) for p in iop_sources + iop_headers})
    if args.target in NATIVE_TARGETS:
        report["native_source_boundary"] = validate_native_boundary([ROOT / name for name in fingerprints], args.target, includes)
        report["shared_engine"] = shared_engine_manifest(fingerprints)
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
        {"toolchain": {key: value for key, value in report["toolchain"].items()
                       if key != "dvp_assembler_version"}, "flags": common},
        sort_keys=True).encode()).hexdigest()

    def compile_source(source: Path) -> Path:
        name = source.stem + "-" + hashlib.sha256(relative(source).encode()).hexdigest()[:12]
        obj = args.output / "obj" / (name + ".o")
        stamp = obj.with_suffix(".json")
        assembly = source.suffix == ".vsm"
        listing = obj.with_suffix(".lst")
        assembly_flags = ["-I" + runner.path(p) for p in [source.parent, *includes]]
        context = hashlib.sha256(json.dumps({"toolchain": report["toolchain"],
                    "assembler": runner.dvp_tool("as"), "flags": assembly_flags,
                    "driver_sha256": digest(Path(__file__))}, sort_keys=True).encode()).hexdigest() if assembly else common_signature
        signature = compilation_key(source, dependencies[source], fingerprints, context)
        if obj.exists() and stamp.exists():
            cached = json.loads(stamp.read_text())
            listing_valid = not assembly or (listing.is_file() and cached.get("listing_sha256") == digest(listing))
            if cached.get("signature") == signature and cached.get("sha256") == digest(obj) and listing_valid:
                return obj
        if assembly:
            runner.run([runner.dvp_tool("as"), *assembly_flags, "-al=" + runner.path(listing),
                        runner.path(source), "-o", runner.path(obj)], "assemble-" + name)
        else:
            cpp = source.suffix not in (".c", ".S", ".s")
            standard = ["-std=c++20"] if cpp else (["-std=gnu11"] if source.suffix == ".c" else [])
            runner.run([runner.tool("g++" if cpp else "gcc"), *standard, *common,
                        "-c", runner.path(source), "-o", runner.path(obj)], "compile-" + name)
        metadata = {"schema": 3, "signature": signature, "sha256": digest(obj),
                    "kind": "dvp_assembly" if assembly else "ee_compilation"}
        if assembly:
            metadata["listing_sha256"] = digest(listing)
        write_json(stamp, metadata)
        return obj

    try:
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            objects = list(pool.map(compile_source, sources))
        report["dvp_objects"] = {relative(source): {"object": relative(obj), "sha256": digest(obj),
            "listing": relative(obj.with_suffix(".lst")), "listing_sha256": digest(obj.with_suffix(".lst"))}
            for source, obj in zip(sources, objects) if source.suffix == ".vsm"}
        modules = spec.get("iop_modules", ["sio2man", "padman"] if spec["embed_iop_modules"] else [])
        pcm_module = build_pcm_module(runner) if spec.get("pcm_transport") else None
        if modules:
            for module in modules:
                obj = args.output / "obj" / (module + "_irx.o")
                generated = obj.with_suffix(".c")
                module_path = runner.path(pcm_module) if module == "rally_pcm" and pcm_module else runner.sdk + "/iop/irx/" + module + ".irx"
                runner.run([runner.sdk + "/bin/bin2c", module_path,
                            runner.path(generated), module + "_irx"], "embed-" + module)
                runner.run([runner.tool("gcc"), "-D_EE", "-G0", "-O2", "-c",
                            runner.path(generated), "-o", runner.path(obj)], "compile-" + module)
                objects.append(obj)
        elf = args.output / elf_name(args.target)
        link = [runner.tool("g++"), "-G0", "-" + args.optimization,
                "-T" + runner.sdk + "/ee/startup/linkfile", "-L" + runner.sdk + "/ee/lib",
                "-Wl,-zmax-page-size=128", "-Wl,--gc-sections",
                "-Wl,-Map," + runner.path(elf.with_suffix(".map")), "-o", runner.path(elf)]
        link += [runner.path(p) for p in objects]
        link += ["-Wl,--start-group", *["-l" + value for value in spec["libraries"]], "-Wl,--end-group"]
        runner.run(link, "link")
        if args.target in NATIVE_TARGETS:
            symbols = runner.run([runner.tool("nm"), "--defined-only", "--demangle", runner.path(elf)], "native-symbol-boundary")
            validate_native_symbols(symbols)
            report["native_source_boundary"]["defined_symbol_audit_passed"] = True
        verify_source_snapshot(fingerprints)
        report["elf"] = inspect_elf(elf)
        report["size"] = runner.run([runner.tool("size"), runner.path(elf)], "size").strip()
        report["status"] = "cross_compiled"
        print(f"Built {relative(elf)} ({report['elf']['file_bytes']:,} bytes); hardware execution unverified.")
    finally:
        report["commands"] = runner.commands


def build_pcm_module(runner: Runner) -> Path:
    """Build our original IOP server against the immutable open PS2SDK."""
    output = runner.output / "iop"
    output.mkdir(exist_ok=True)
    prefix = runner.ps2dev + "/iop/bin/mipsel-none-elf-"
    flags = ["-D_IOP", "-G0", "-Os", "-Wall", "-Wextra", "-Werror", "-fno-builtin",
             "-msoft-float", "-mno-explicit-relocs", "-fno-toplevel-reorder",
             "-I" + runner.sdk + "/iop/include", "-I" + runner.sdk + "/common/include",
             "-I" + runner.path(HERE / "include")]
    objects = []
    for source in ("pcm_server", "imports"):
        obj = output / (source + ".o")
        runner.run([prefix + "gcc", *flags, "-c", runner.path(HERE / "iop" / (source + ".c")),
                    "-o", runner.path(obj)], "iop-compile-" + source)
        objects.append(obj)
    linked = output / "rally_pcm.elf"
    runner.run([prefix + "gcc", "-G0", "-msoft-float", "-nostdlib", "-r", "-Wl,-d",
                "-T" + runner.sdk + "/iop/startup/linkfile", "-o", runner.path(linked),
                *[runner.path(p) for p in objects]], "iop-link")
    stripped = output / "rally_pcm.stripped.elf"
    runner.run([prefix + "strip", "--strip-unneeded", "--remove-section=.pdr",
                "--remove-section=.comment", "--remove-section=.mdebug.abi32",
                "--remove-section=.gnu.attributes", "-o", runner.path(stripped), runner.path(linked)], "iop-strip")
    module = output / "rally_pcm.irx"
    runner.run([runner.sdk + "/bin/iopfixup", "--rb", "--irx1", "--allow-zero-text",
                "-o", runner.path(module), runner.path(stripped)], "iop-module")
    return module


def smoke(args: argparse.Namespace, report: dict) -> None:
    if args.target in NATIVE_TARGETS:
        # The native bootstrap has its own portable unit tests. This command
        # checks its complete source boundary without executing a reference
        # rasterizer or requiring the cross compiler.
        spec = build_spec(args)
        sources = [native_source_path(name) for name in [*spec["sources"], *NATIVE_IOP_SOURCES]]
        includes = [project_path(name) for name in spec["includes"]]
        headers = sorted(set(project_headers(sources + engine_header_paths(), includes) + engine_header_paths()))
        report["native_source_boundary"] = validate_native_boundary(sources + headers, args.target, includes)
        report.update(status="native_source_boundary_passed", executable_run=False,
                      source_sha256={relative(path): digest(path) for path in sources + headers})
        report["shared_engine"] = shared_engine_manifest(report["source_sha256"])
        print("Native PS2 source boundary passed; this command does not execute gameplay or rendering.")
        return
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
    parser.add_argument("--target", choices=("bringup", "audio-fixture", "native-port", "engine-demo", "core-probe"), default="bringup")
    parser.add_argument("--backend", choices=("docker", "native"), default="docker")
    parser.add_argument("--output", type=project_path, default=ROOT / "build/ps2-bringup")
    parser.add_argument("--ps2dev", type=Path, default=Path(os.environ.get("PS2DEV", "/usr/local/ps2dev")))
    parser.add_argument("--container-platform", choices=tuple(LOCK["platforms"]))
    parser.add_argument("--core-manifest", type=project_path)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--optimization", choices=("O2", "O3", "Os"),
                        help="defaults to O3 for native-port, O2 for other targets")
    parser.add_argument("--host-cxx", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args(argv)
    if args.optimization is None:
        args.optimization = "O3" if args.target == "native-port" else "O2"
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
            elf = args.output / elf_name(args.target)
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
