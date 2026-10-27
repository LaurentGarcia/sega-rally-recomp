"""ROM-free whole-callback continuity, access forwarding and rejection checks."""
import copy
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"platform/ps2/tools"))
import full_step_capture as capture
import extract_main_motion as motion


class FullStepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler: raise unittest.SkipTest("C++20 compiler required")
        (ROOT/"build").mkdir(exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(prefix="full-step-test-",dir=ROOT/"build")
        cls.directory=Path(cls.temp.name);cls.output=cls.directory/"capture";cls.output.mkdir()
        exe=cls.directory/"fixture"
        result=subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
            "-I",str(ROOT/"src"),"-I",str(ROOT/"platform/ps2/tools"),
            str(ROOT/"platform/ps2/tools/full_step_capture.cpp"),str(ROOT/"platform/ps2/tests/full_step_capture_test.cpp"),
            "-o",str(exe)],capture_output=True,text=True)
        if result.returncode: raise AssertionError(result.stdout+result.stderr)
        result=subprocess.run([str(exe),str(cls.output)],capture_output=True,text=True)
        if result.returncode: raise AssertionError(result.stdout+result.stderr)
        if result.stdout!="FULL_STEP_CAPTURE PASS steps=2 device_reads=2\n": raise AssertionError(result.stdout)
        cls.events=[json.loads(s) for s in (cls.output/"full-step.jsonl").read_text().splitlines()]
        cls.summary=json.loads((cls.output/"full-step-summary.json").read_text())

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def replay(self,events=None,summary=None,access=None,seed=None):
        marker=summary or self.summary
        return list(capture.replay_steps(capture.step_envelopes(events or self.events,marker),marker,
            seed or self.output/"full-step-seed.bin",access or self.output/"full-step-access.bin"))

    def test_forwarding_suppression_and_continuous_partial_write_state(self):
        steps=self.replay()
        self.assertEqual(len(steps),2)
        self.assertEqual(bytes(steps[0]["read_before_write"][a] for a in range(0x500010,0x500014)),b"\x44\x33\x22\x11")
        self.assertEqual(bytes(steps[1]["read_before_write"][a] for a in range(0x500010,0x500014)),b"\x44\xee\xcc\x55")
        self.assertEqual(steps[1]["external_writes"],{0x500011:0xee,0x500012:0xcc,0x500013:0x55})
        self.assertEqual(len(steps[0]["device_transactions"]),1)
        self.assertEqual(len(steps[1]["device_transactions"]),1)
        # Original accesses plus the final code guard; snapshot read excluded.
        self.assertEqual(self.summary["transactions"],15)
        self.assertEqual(steps[0]["boundaries"][1]["controls"][4],0x80)
        self.assertEqual(steps[1]["boundaries"][1]["controls"][4],0x60)

    def test_missing_reordered_duplicate_or_inconsistent_parent_evidence(self):
        mutations=[lambda e:e.pop(),lambda e:e.insert(0,copy.deepcopy(e[0])),
            lambda e:e[4].update(frame=13),lambda e:e[2].update(pc=0x5d5764),
            lambda e:e[3]["parent_return"]["words"].__setitem__(2,0x5c9ba8),
            lambda e:e[2].update(helper_frame_pointer=0x510400),
            lambda e:e[1]["registers"].__setitem__(17,0x510084),
            lambda e:e[2]["controls"].__setitem__(4,0),
            lambda e:e[1]["blocks"].append(copy.deepcopy(e[1]["blocks"][0]))]
        for mutation in mutations:
            events=copy.deepcopy(self.events);mutation(events)
            with self.subTest(mutation=mutation),self.assertRaises(ValueError): self.replay(events=events)

    def test_ordinary_read_mismatch_and_snapshot_mutation_are_not_hidden(self):
        original=bytearray((self.output/"full-step-access.bin").read_bytes())
        changed=self.directory/"changed.bin"
        struct.pack_into("<I",original,8+16,0x11223345);changed.write_bytes(original)
        with self.assertRaisesRegex(ValueError,"Unaccounted ordinary-memory mutation"): self.replay(access=changed)
        events=copy.deepcopy(self.events);events[-1]["blocks"][0]["words"][10]^=1
        with self.assertRaisesRegex(ValueError,"Untraced memory change"): self.replay(events=events)

    def test_transaction_bounds_external_classification_and_trailing_data(self):
        original=(self.output/"full-step-access.bin").read_bytes();changed=self.directory/"bad.bin"
        for data in (original[:-1],original+b"x",b"BROKEN00"+original[8:]):
            changed.write_bytes(data)
            with self.assertRaises(ValueError): self.replay(access=changed)
        data=bytearray(original);struct.pack_into("<I",data,8+20,4|0x300);changed.write_bytes(data)
        with self.assertRaises(ValueError): self.replay(access=changed)
        data=bytearray(original);struct.pack_into("<I",data,8+20,3);changed.write_bytes(data)
        with self.assertRaises(ValueError): self.replay(access=changed)
        marker=copy.deepcopy(self.summary);marker["steps"]+=1
        with self.assertRaises(ValueError): self.replay(summary=marker)

    def test_reconstruction_callbacks_expose_exact_readonly_state_before_final_instruction(self):
        observations=[]
        def before(record,memory):
            if record[1]==motion.END_PC:
                self.assertTrue(memory.readonly)
                observations.append(struct.unpack_from("<I",memory,capture.offset(0x214200,4))[0])
        list(capture.replay_steps(capture.step_envelopes(self.events,self.summary),self.summary,
            self.output/"full-step-seed.bin",self.output/"full-step-access.bin",on_transaction=before))
        self.assertEqual(observations,[7,8])

    def test_compact_tail_extraction_includes_preserved_conditional_words_and_requires_final_boundary(self):
        paths={name:self.output/name for name in ("full-step-seed.bin","full-step-access.bin")}
        groups=capture.step_envelopes(self.events,self.summary);out=io.BytesIO()
        result=motion.extract(groups,self.summary,paths,out)
        self.assertEqual(len(result["records"]),2)
        self.assertEqual(out.getvalue()[:12],b"MOTTAIL1\x02\x00\x00\x00")
        data=out.getvalue();cursor=12
        for frame in (10,11):
            observed,fp,obj,count=struct.unpack_from("<4I",data,cursor);cursor+=16
            self.assertEqual((observed,fp,obj),(frame,0x510040,0x500000));addresses=set()
            for _ in range(count):
                address,n=struct.unpack_from("<2I",data,cursor);cursor+=8
                addresses.update(range(address,address+4*n,4));cursor+=8*n
            self.assertTrue({0x214360,0x214368,0x2139f8,0x20b184,0x202048}<=addresses)
            queued=struct.unpack_from("<I",data,cursor)[0];cursor+=4+8*queued
            self.assertEqual(queued,0)
        self.assertEqual(cursor,len(data))
        changed=self.directory/"no-final.bin";raw=bytearray(paths["full-step-access.bin"].read_bytes())
        for pos in range(8,len(raw),24):
            if struct.unpack_from("<I",raw,pos+8)[0]==motion.END_PC: struct.pack_into("<I",raw,pos+8,motion.END_PC+4)
        changed.write_bytes(raw)
        with self.assertRaisesRegex(ValueError,"final boundary"):
            motion.extract(groups,self.summary,dict(paths,**{"full-step-access.bin":changed}),io.BytesIO())

    def test_original_enqueue_overflow_is_rejected_and_queue_order_is_preserved(self):
        paths={name:self.output/name for name in ("full-step-seed.bin","full-step-access.bin")}
        changed=self.directory/"overflow.bin";raw=bytearray(paths["full-step-access.bin"].read_bytes())
        for pos in range(8,len(raw),24):
            if struct.unpack_from("<I",raw,pos+8)[0]==motion.END_PC:
                raw[pos:pos]=capture.RECORD.pack(105,0x5c50a4,0x5c50a4,0,4);break
        changed.write_bytes(raw)
        marker=copy.deepcopy(self.summary);marker["transactions"]+=1
        events=copy.deepcopy(self.events)
        for e in events[3:]: e["transaction_index"]+=1
        with self.assertRaisesRegex(ValueError,"queue overflow"):
            motion.extract(capture.step_envelopes(events,marker),marker,dict(paths,**{"full-step-access.bin":changed}),io.BytesIO())
        memory=bytearray(0x140000)
        encoded=motion.compact_record(1,0x510040,0x500000,memory,memory,{0x500010},[(0x20b17f,1),(0x20b100,2)])
        self.assertEqual(encoded[-20:],struct.pack("<5I",2,0x20b17f,1,0x20b100,2))


if __name__=="__main__": unittest.main()
