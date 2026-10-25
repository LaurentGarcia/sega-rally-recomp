#!/usr/bin/env python3
"""ROM-free checks of the actual offline observer and its output boundary."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]


class DrivingCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler=shutil.which("clang++") or shutil.which("c++")
        if not compiler: raise unittest.SkipTest("C++20 compiler required")
        (ROOT/"build").mkdir(exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(prefix="driving-observer-test-",dir=ROOT/"build")
        cls.directory=Path(cls.temp.name)
        cls.exe=cls.directory/"observer"
        result=subprocess.run([compiler,"-std=c++20","-O2","-Wall","-Wextra","-Werror",
            "-I",str(ROOT/"src"),"-I",str(ROOT/"platform/ps2/tools"),
            str(ROOT/"platform/ps2/tools/driving_capture.cpp"),
            str(ROOT/"platform/ps2/tests/driving_capture_test.cpp"),"-o",str(cls.exe)],
            capture_output=True,text=True)
        if result.returncode: raise AssertionError(result.stdout+result.stderr)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def run_case(self,mode,name=None):
        path=self.directory/(name or mode)
        return path,subprocess.run([str(self.exe),mode,str(path)],capture_output=True,text=True)

    def test_actual_records_raw_bits_pairing_and_optional_block_bounds(self):
        for mode in ("valid","rom","alternate"):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertEqual(result.returncode,0,result.stderr)
                marker=json.loads((path/"capture.json").read_text())
                self.assertEqual((marker["calls"],marker["completed_calls"],marker["events"]),(1,1,10))
                self.assertEqual(marker["alternate_calls"],int(mode=="alternate"))
                self.assertEqual(marker["selector_observations"],1)
                self.assertEqual(marker["callback_observations"],1)
                events=[json.loads(line) for line in (path/"events.jsonl").read_text().splitlines()]
                self.assertEqual([e["event"] for e in events],["integrator_entry","orientation_pre",
                    "orientation_post","blend_pre","blend_store","blend_post","query_pre",
                    "query_count","query_post","integrator_return"])
                self.assertEqual(events[0]["fp_bits"][0],1<<63)
                self.assertEqual(events[0]["object_words"][0],0x210000^0x12345678)
                self.assertTrue(all(len(e["registers"])==32 and len(e["object_words"])==256 for e in events))
                self.assertEqual(events[0]["registers"][16],events[0]["object"])
                self.assertEqual(events[0]["branch_selector"],events[0]["object_words"][0xe8//4])
                entry_blocks={b["name"]:b for b in events[0]["blocks"]}
                self.assertEqual((entry_blocks["object_pointer_globals"]["address"],
                                  len(entry_blocks["object_pointer_globals"]["words"])),(0x2140c0,16))
                self.assertEqual((entry_blocks["baseline_object"]["address"],
                                  len(entry_blocks["baseline_object"]["words"])),(0x500000,64))
                callback=json.loads((path/"callbacks.jsonl").read_text())
                self.assertEqual(callback["object"],events[0]["object"])
                self.assertEqual(callback["callback"],0x34f40+(0 if mode=="rom" else 0x59f000))
                selector=json.loads((path/"selectors.jsonl").read_text())
                self.assertEqual((selector["object"],selector["selector"]),
                                 (events[0]["object"],events[0]["branch_selector"]))
                self.assertEqual(events[-2]["pc"],events[-4]["registers"][2])
                blocks={b["name"]:b for b in events[3]["blocks"]}
                self.assertEqual(blocks["blend_output"]["address"],0x220000)
                self.assertEqual(len(blocks["blend_output"]["words"]),4)
                self.assertNotIn("selected_descriptor",blocks)
                self.assertNotIn("query_staging",blocks)
                self.assertEqual(len((path/"cadence.jsonl").read_text().splitlines()),2)

    def test_alternate_orientation_has_ordered_boundaries_and_four_raw_candidate_words(self):
        path,result=self.run_case("alternate-orientation")
        self.assertEqual(result.returncode,0,result.stderr)
        marker=json.loads((path/"capture.json").read_text())
        self.assertEqual((marker["alternate_orientation_entries"],marker["alternate_orientation_completions"]),(1,1))
        events=[json.loads(line) for line in (path/"events.jsonl").read_text().splitlines()]
        events=[event for event in events if event["event"].startswith("alternate_orientation_")]
        self.assertEqual([e["event"] for e in events],
                         ["alternate_orientation_pre","alternate_orientation_folded","alternate_orientation_post"])
        self.assertEqual([e["pc"] for e in events],[0x5cdcb4,0x5cdce0,0x5cde00])
        blocks=[next(b for b in event["blocks"] if b["name"]=="orientation_candidate") for event in events]
        for event,block in zip(events,blocks):
            self.assertEqual(block["address"],event["registers"][31]+0x90)
            self.assertEqual(block["address"],event["registers"][5])
            self.assertEqual(event["registers"][12],event["object"])
            self.assertEqual(len(block["words"]),4)
            self.assertEqual(block["words"][3],0x3f000004)
        self.assertEqual(blocks[0]["words"][:3],[0x3f000001+i for i in range(3)])
        self.assertEqual(blocks[1]["words"][:3],[0x3e000001+i for i in range(3)])
        self.assertEqual(blocks[2]["words"],blocks[1]["words"])
        self.assertEqual(events[0]["object_words"][14:17],[0xbf000001+i for i in range(3)])
        self.assertEqual(events[2]["object_words"][14:17],[0xbe000001+i for i in range(3)])

    def test_contact_reply_order_and_saved_output_identity(self):
        path,result=self.run_case("contact")
        self.assertEqual(result.returncode,0,result.stderr)
        marker=json.loads((path/"capture.json").read_text())
        self.assertEqual((marker["contact_entries"],marker["contact_completions"]),(1,1))
        events=[json.loads(line) for line in (path/"events.jsonl").read_text().splitlines()]
        events=[event for event in events if event["event"].startswith("contact_")]
        self.assertEqual([e["event"] for e in events],["contact_pre","contact_reply","contact_post"])
        self.assertEqual([e["pc"] for e in events],[0x5c9cc0,0x5c9dac,0x5cd8a8])
        blocks=[{b["name"]:b for b in event["blocks"]} for event in events]
        reply=blocks[1]["contact_reply_prefix"]["words"]+blocks[1]["contact_reply_tail"]["words"]+[events[1]["registers"][21]]
        self.assertEqual(reply,list(range(100,117)))
        self.assertEqual(blocks[2]["contact_output"]["words"],list(range(200,208)))
        self.assertEqual(blocks[2]["contact_output"]["address"],events[0]["registers"][17])
        self.assertNotEqual(blocks[2]["contact_output"]["address"],events[2]["registers"][17])
        self.assertEqual(blocks[2]["contact_point"]["address"],events[0]["registers"][16])

    def test_steering_has_independent_identity_without_a_fabricated_object(self):
        path,result=self.run_case("steering")
        self.assertEqual(result.returncode,0,result.stderr)
        marker=json.loads((path/"capture.json").read_text())
        self.assertEqual((marker["steering_entries"],marker["steering_completions"],marker["steering_samples"]),(1,1,1))
        events=[json.loads(line) for line in (path/"steering.jsonl").read_text().splitlines()]
        self.assertEqual([event["event"] for event in events],["steering_pre","steering_post"])
        for event in events:
            self.assertNotIn("object",event)
            self.assertEqual(event["input_address"],0x202050)
            self.assertEqual(event["input_byte"],0x53)
            self.assertEqual(event["frame_pointer"],event["registers"][31])
        self.assertEqual(events[0]["return_pc"],events[0]["registers"][30])
        self.assertEqual(events[1]["return_pc"],events[1]["registers"][17])
        self.assertEqual(events[1]["registers"][16],0xbe123456)
        self.assertEqual(events[0]["pair_id"],events[1]["pair_id"])

    def test_contact_pairing_failure_saves_bounded_return_state_without_success(self):
        for mode in ("contact-incomplete","contact-return-wrong-frame"):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode,"diagnostic-"+mode)
                self.assertNotEqual(result.returncode,0)
                diagnostic=json.loads((path/"contact-failure.json").read_text())
                self.assertIs(diagnostic["complete"],False)
                self.assertEqual(diagnostic["saved_return"],0x5cd8a8)
                self.assertEqual(diagnostic["saved_caller_fp"],0x500040)
                self.assertEqual(diagnostic["saved_helper_fp"],0x500200)
                self.assertEqual(diagnostic["last_ret_pc"],0x5c9e68)
                self.assertEqual(diagnostic["last_ret_rip"],0x5cd8a8)
                self.assertEqual(diagnostic["last_ret_pfp"],0x500040)
                self.assertTrue(diagnostic["reply_seen"])
                self.assertFalse((path/"capture.json").exists())

    def test_actual_saved_caller_rip_survives_irq_and_cache_spill(self):
        cases=(("outer-interrupted-entry","integrator_entry","integrator_return",0x5cafec),
               ("query-interrupted-entry","query_pre","query_post",0x5cec30),
               ("contact-interrupted-entry","contact_pre","contact_post",0x5cd8a8),
               ("contact-spilled","contact_pre","contact_post",0x5cd8a8),
               ("all-spilled","integrator_entry","integrator_return",0x5cafec))
        for mode,pre_name,post_name,pc in cases:
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertEqual(result.returncode,0,result.stderr)
                events=[json.loads(line) for line in (path/"events.jsonl").read_text().splitlines()]
                pre=next(event for event in events if event["event"]==pre_name)
                post=next(event for event in events if event["event"]==post_name)
                saved=pre["return_frame"]
                self.assertEqual(saved["pc"],pc)
                self.assertEqual(saved["words"][2],pc)
                self.assertEqual(saved["frame_pointer"],pre["registers"][0]&~63)
                self.assertEqual(post["pc"],pc)
                self.assertEqual(post["registers"][31],saved["frame_pointer"])
                spilled="spilled" in mode
                self.assertEqual(saved["source"],"ordinary_ram" if spilled else "register_cache")
                self.assertEqual(saved["cache_index"],-1 if spilled else 0)
                if "interrupted" in mode: self.assertNotEqual(pre["registers"][2],pc)

    def test_filter_pairs_history_and_input_independently_of_inner_leaf(self):
        path,result=self.run_case("filter")
        self.assertEqual(result.returncode,0,result.stderr)
        marker=json.loads((path/"capture.json").read_text())
        self.assertEqual((marker["steering_filter_entries"],marker["steering_filter_completions"],
                          marker["steering_filter_samples"]),(1,1,1))
        events=[json.loads(line) for line in (path/"steering-filter.jsonl").read_text().splitlines()]
        self.assertEqual([event["event"] for event in events],["steering_filter_pre","steering_filter_post"])
        self.assertEqual([event["pc"] for event in events],[0x5d8a90,0x5d8ae8])
        blocks=[{b["name"]:b for b in event["blocks"]} for event in events]
        self.assertEqual(blocks[0]["steering_history"]["words"],[0x3e000000])
        self.assertEqual(blocks[1]["steering_history"]["words"],[0xbe000000])
        self.assertEqual(blocks[0]["steering_input"]["words"],[0x12345680])
        self.assertEqual(events[1]["registers"][16],0x3e000000)
        self.assertEqual(events[0]["return_frame"],events[1]["return_frame"])
        self.assertTrue(all("object" not in event for event in events))

    def test_failures_never_seal_a_capture(self):
        for mode,message in (("forbidden","non-RAM"),("nested","nested"),
                             ("timeout","bounded instruction"),("incomplete","no complete"),
                             ("unselected","no complete"),("nested-query","unpaired table query"),
                             ("unfinished-query","unpaired query")):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertNotEqual(result.returncode,0)
                self.assertIn(message,result.stderr)
                self.assertFalse((path/"capture.json").exists())

    def test_new_real_build_directory_is_required(self):
        existing=self.directory/"existing";existing.mkdir()
        with tempfile.TemporaryDirectory(prefix="driving-outside-") as external:
            linked=self.directory/"alias";linked.symlink_to(external,target_is_directory=True)
            for path in (existing,ROOT/"build",Path(external)/"capture",linked/"capture"):
                result=subprocess.run([str(self.exe),"valid",str(path)],capture_output=True,text=True)
                self.assertNotEqual(result.returncode,0)
                self.assertIn("new directory below ignored build",result.stderr)
            self.assertFalse((Path(external)/"capture").exists())

    def test_main_pass_has_independent_four_slot_pipeline_and_exact_cross_blocks(self):
        for mode in ("main-valid","main-spilled","main-no-cross"):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertEqual(result.returncode,0,result.stderr)
                marker=json.loads((path/"capture.json").read_text())
                self.assertEqual((marker["main_calls"],marker["main_completions"],marker["main_samples"]),(1,1,1))
                self.assertEqual([marker[name] for name in ("main_queries","main_query_completions",
                    "main_contacts","main_contact_completions")],[4]*4)
                crosses=0 if mode=="main-no-cross" else 4
                self.assertEqual((marker["main_cross_pairs"],marker["main_sampled_cross_pairs"]),(crosses,crosses))
                events=[json.loads(line) for line in (path/"main-contact.jsonl").read_text().splitlines()]
                self.assertEqual(len(events),31+2*crosses)
                self.assertEqual(marker["main_events"],len(events))
                self.assertEqual([events[0]["event"],events[1]["event"],events[-1]["event"]],
                                 ["main_entry","main_prequery","main_return"])
                self.assertEqual(events[-1]["pc"],events[0]["return_frame"]["pc"])
                self.assertEqual(events[0]["return_frame"]["pc"],0x5d421c)
                self.assertNotEqual(events[0]["registers"][2],0x5d421c)
                self.assertEqual([event["instructions"] for event in events],
                                 sorted(set(event["instructions"] for event in events)))
                self.assertEqual(len((path/"main-contact-cadence.jsonl").read_text().splitlines()),2)
                self.assertEqual(marker["events"],10)  # Existing alternate stream remains independent.
                for kind,index_key in (("query_pre","query_index"),("contact_pre","contact_index"),
                                       ("response_post","contact_index"),("cross_pre","cross_index")):
                    self.assertEqual([e[index_key] for e in events if e["event"]==kind],
                                     list(range(crosses if kind=="cross_pre" else 4)))
                for event in events:
                    self.assertEqual(event["parent_return_frame"],events[0]["return_frame"])
                    self.assertEqual(event["object"],0x210000)
                    if event["event"] in ("query_pre","contact_pre"):
                        proof=event["return_frame"]
                        self.assertEqual(proof["frame_pointer"],0x500400)
                        self.assertEqual(proof["words"][2],proof["pc"])
                        self.assertNotEqual(proof["pc"],event["registers"][2])
                        self.assertEqual(proof["source"],"ordinary_ram" if mode=="main-spilled" else "register_cache")
                    blocks={block["name"]:block for block in event["blocks"]}
                    self.assertEqual(blocks["candidate_position"]["address"],0x220000)
                    self.assertEqual(blocks["candidate_orientation"]["address"],0x220020)
                    self.assertEqual(len(blocks["result_slots"]["words"]),36)
                    if event["event"]=="main_prequery":
                        self.assertEqual(blocks["query_points"]["words"],list(range(1000,1012)))
                        self.assertEqual(blocks["transformed_offsets"]["words"],list(range(2000,2012)))
                    if event["event"]=="contact_reply":
                        i=event["contact_index"]
                        reply=blocks["contact_reply_prefix"]["words"]+blocks["contact_reply_tail"]["words"]+[event["registers"][21]]
                        self.assertEqual(reply,list(range(100*i,100*i+17)))
                    if event["event"]=="contact_post":
                        i=event["contact_index"]
                        self.assertEqual(blocks["contact_output"]["address"],0x2138d0+36*i)
                        self.assertEqual(blocks["contact_output"]["words"],list(range(4000+100*i,4008+100*i)))
                    if event["event"].startswith("cross_"):
                        i=event["cross_index"]
                        self.assertEqual(blocks["cross_left"]["words"],list(range(2000+3*i,2003+3*i)))
                        self.assertEqual(blocks["cross_right"]["words"],list(range(3000+3*i,3003+3*i)))
                        self.assertEqual(blocks["cross_result"]["address"],0x5004d0)
                        if event["event"]=="cross_post":
                            self.assertEqual(blocks["cross_result"]["words"],list(range(5000+100*i,5003+100*i)))

    def test_main_pass_rejects_missing_boundaries_wrong_identity_and_non_ram(self):
        for suffix in ("bad-position","bad-orientation","bad-stack","bad-caller","nested","timeout",
                       "prequery-object","no-prequery","duplicate-prequery","query-point","query-lane",
                       "query-return","duplicate-query","query-no-count","query-duplicate-count","query-wrong-frame",
                       "contact-slot","contact-point","contact-return","contact-no-reply","contact-duplicate-reply",
                       "contact-wrong-frame","response-index","response-missing","cross-offset","cross-pointer",
                       "cross-object","cross-frame","cross-no-pre","cross-duplicate-pre","cross-no-post",
                       "cross-partial","return-frame","incomplete"):
            with self.subTest(suffix=suffix):
                path,result=self.run_case("main-"+suffix)
                self.assertNotEqual(result.returncode,0,result.stdout)
                self.assertFalse((path/"capture.json").exists())
                self.assertFalse((path/"main-contact.jsonl").exists())

    def test_main_aggregate_window_is_sampled_without_expanding_every_frame(self):
        for mode,sampled in (("main-aggregate-window",True),("main-before-aggregate-window",False)):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertEqual(result.returncode,0,result.stderr)
                marker=json.loads((path/"capture.json").read_text())
                self.assertEqual(marker["main_calls"],1)
                self.assertEqual(marker["main_samples"],int(sampled))
                self.assertEqual(marker["main_cross_pairs"],4)
                self.assertEqual(marker["main_sampled_cross_pairs"],4 if sampled else 0)
                events=[json.loads(line) for line in (path/"main-contact.jsonl").read_text().splitlines()]
                self.assertEqual(len(events),39 if sampled else 0)
                if sampled: self.assertTrue(all(event["frame"]==2571 for event in events))

    def test_new_region_failures_never_seal_a_capture(self):
        cases={
            "orientation-bad-pointer":"input frame or object mismatch",
            "orientation-bad-object":"input frame or object mismatch",
            "orientation-overflow":"input frame or object mismatch",
            "orientation-nonram":"input frame or object mismatch",
            "orientation-duplicate-pre":"unique alternate caller",
            "orientation-wrong-frame":"sequence or identity mismatch",
            "orientation-missing-pre":"sequence or identity mismatch",
            "orientation-missing-fold":"sequence or identity mismatch",
            "orientation-incomplete":"incomplete alternate orientation",
            "contact-bad-pointer":"bounded ordinary RAM",
            "contact-bad-frame":"bounded ordinary RAM",
            "contact-duplicate-pre":"unpaired contact helper",
            "contact-missing-pre":"matching helper frame",
            "contact-wrong-frame":"matching helper frame",
            "contact-duplicate-reply":"matching helper frame",
            "contact-missing-reply":"without its reply observation",
            "contact-incomplete":"unpaired contact helper",
            "contact-return-wrong-frame":"different frame",
            "steering-nested":"nested steering",
            "steering-wrong-return":"steering leaf return or frame mismatch",
            "steering-wrong-frame":"steering leaf return or frame mismatch",
            "steering-incomplete":"no complete bounded caller evidence",
            "contact-cache-mismatch":"register-cache frame mismatch",
            "contact-caller-nonram":"saved caller frame is outside plain RAM",
            "contact-return-unaligned":"aligned return address",
            "filter-nested":"nested steering filter",
            "filter-wrong-frame":"steering filter return or frame mismatch",
            "filter-wrong-return":"steering filter return or frame mismatch",
            "filter-incomplete":"no complete bounded caller evidence",
        }
        for mode,message in cases.items():
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertNotEqual(result.returncode,0)
                self.assertIn(message,result.stderr)
                self.assertFalse((path/"capture.json").exists())


if __name__=="__main__": unittest.main()
