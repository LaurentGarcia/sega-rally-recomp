"""Original fixtures for the native driving asset boundary; no game data."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "platform/ps2/tools/export_driving_assets.py"
spec = importlib.util.spec_from_file_location("driving_export", TOOL)
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)

def original_data():
    vertices = [(0xbf800000,0xbf800000),(0x3f800000,0xbf800000),(0x3f800000,0x3f800000),(0xbf800000,0x3f800000)]
    polygon = [word for x,z in vertices for word in (x,0x7fc12345,z)] + [0xff800001,0x80000000,0x7f800000,0x01234567,0xabcdef80]
    return dict(selector=3, vertices=vertices, quads=[(0,1,2,3)], segments=[[polygon]],
                normals=[(0x3f000000,0x40000000)]*8192,
                parameters=[list(range(0x7f800001+13*i,0x7f800001+13*(i+1))) for i in range(10)],
                local=[(0x80000000,0,0),(0,0,0),(0,0,0),(0,0,0)],
                balance_words=[0x3f000000+i if i<212 else 0x7f800000+i for i in range(260)],
                power_curves=[0x40000000+i for i in range(164)],
                motion_words=[0x7f800100+i for i in range(64)]+[0x3f000400],
                motion_local=[tuple(0x3f000500+3*i+j for j in range(3)) for i in range(4)],
                sound_rows=[(0,127,255),(255,128,0)],directions=[(0x80000000,0x3f800000)])

class DrivingAssets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if compiler is None:
            raise unittest.SkipTest("C++20 compiler unavailable")
        cls.temp = tempfile.TemporaryDirectory(prefix="driving-assets-")
        cls.directory = Path(cls.temp.name)
        cls.binary = cls.directory / "test"
        native = ROOT / "platform/ps2/native"
        sources = [native / (name + ".cpp") for name in ("driving_assets","segment_selection","contact_projection","contact_search","contact_neighborhood","vehicle_parameter_selection")]
        sources.append(ROOT / "platform/ps2/tests/driving_assets_test.cpp")
        subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror","-I",str(native),*map(str,sources),"-o",str(cls.binary)],check=True,capture_output=True,text=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_file(self, blob):
        path = self.directory / "original.bin"
        path.write_bytes(blob)
        return subprocess.run([str(self.binary),str(path)],capture_output=True,text=True)

    def test_native_ownership_and_typed_round_trip(self):
        blob = exporter.encode_package(**original_data())
        result = self.run_file(blob)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn("DRIVING_ASSETS PASS checks=120",result.stdout)
        fingerprint = 14695981039346656037
        for byte in blob[96:]:
            fingerprint = ((fingerprint ^ byte) * 1099511628211) & ((1 << 64)-1)
        self.assertIn(f"payload_hash={fingerprint:016x}",result.stdout)

    def test_file_lengths_and_checksums(self):
        good = exporter.encode_package(**original_data())
        for bad in (good[:95],good[:-1],good+b"\0",good[:-1]+b"\1"):
            with self.subTest(size=len(bad)):
                result = self.run_file(bad)
                self.assertNotEqual(result.returncode,0)
                self.assertIn("LOAD FAIL",result.stderr)

    def test_encoder_rejects_invalid_domain(self):
        changes = [dict(selector=-1),dict(selector=True),dict(vertices=[]),dict(quads=[(0,1,2,4)]),
                   dict(normals=[(0xbf000000,0x40000000)]*8192),dict(parameters=[[0]*12]*10),
                   dict(local=[(0x7f800000,0,0)]*4),dict(segments=[[]]),dict(directions=[]),
                   dict(directions=[(0x7f800000,0)]),dict(directions=[(0,0,0)])]
        for changes_here in changes:
            data=original_data();data.update(changes_here)
            with self.subTest(changes=tuple(changes_here)):
                with self.assertRaises(ValueError):exporter.encode_package(**data)
        for bits in (1,0x80000001,0x49800000,0xc9800000):
            data=original_data();data["vertices"][0]=(bits,0)
            exporter.encode_package(**data)
        for field,index,bits in (("balance_words",0,0x7f800000),("power_curves",163,0x7fc00000),("motion_words",64,0xff800000)):
            data=original_data();data[field][index]=bits
            with self.assertRaises(ValueError):exporter.encode_package(**data)
        for rows in ([],[(0,1,256)],[(0,1)],[(0,1,2)]*4097):
            data=original_data();data["sound_rows"]=rows
            with self.assertRaises(ValueError):exporter.encode_package(**data)

    def test_maximum_counts_and_last_corner(self):
        data=original_data();polygon=data["segments"][0][0]
        data["vertices"]=[(0,0)]*65536
        data["quads"]=[(0,1,2,65535)]*4096
        data["segments"]=[[polygon]*16 for _ in range(4096)]
        data["segments"][-1]=[polygon]*15
        data["sound_rows"]=[(0,1,255)]*4096
        data["directions"]=[(0x3f800000,0)]*4096
        blob=exporter.encode_package(**data)
        result=self.run_file(blob)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn("vertices=65536 segments=4096 polygons=65535",result.stdout)

    def test_private_output_boundary(self):
        build=ROOT/"build"
        build.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="driving-output-",dir=build) as directory:
            directory=Path(directory)
            self.assertEqual(exporter.private_destination(directory/"new.bin"),directory/"new.bin")
            (directory/"old.bin").write_bytes(b"original fixture")
            (directory/"sidecar.json").write_text("{}")
            (directory/"link").symlink_to(directory,target_is_directory=True)
            for path in (directory/"old.bin",directory/"sidecar.bin",directory/"wrong.txt",directory/"link/new.bin",ROOT/"outside.bin"):
                with self.subTest(path=path):
                    with self.assertRaises(ValueError):exporter.private_destination(path)

    def test_source_ranges_fail_closed(self):
        for offset,count in ((-4,1),(1,1),(4,2),(0,-1)):
            with self.assertRaises(ValueError):exporter.words(bytes(8),offset,count)
        self.assertEqual(exporter.words(bytes(8),8,0),())

if __name__ == "__main__":
    unittest.main()
