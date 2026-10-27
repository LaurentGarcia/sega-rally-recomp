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
            str(ROOT/"platform/ps2/tools/full_step_capture.cpp"),
            str(ROOT/"platform/ps2/tests/driving_capture_test.cpp"),"-o",str(cls.exe)],
            capture_output=True,text=True)
        if result.returncode: raise AssertionError(result.stdout+result.stderr)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def run_case(self,mode,name=None):
        path=self.directory/(name or mode)
        return path,subprocess.run([str(self.exe),mode,str(path)],capture_output=True,text=True)

    def test_additive_motion_stream_preserves_saved_pointer_outputs_and_miss_indices(self):
        for mode in ("vehicle-motion-valid","vehicle-motion-spilled"):
            path,result=self.run_case(mode)
            self.assertEqual(result.returncode,0,result.stderr)
            marker=json.loads((path/"capture.json").read_text())
            events=[json.loads(line) for line in (path/"main-motion.jsonl").read_text().splitlines()]
            self.assertEqual([e["event"] for e in events],
                             ["segment_pre","segment_post","motion_tail_pre","motion_tail_post"])
            self.assertEqual([e["pc"] for e in events],[0x5ca0b0,0x5d3fc8,0x5d72f8,0x5d7658])
            self.assertEqual(marker["motion_events"],4)
            for kind in ("segment","tail"):
                self.assertEqual([marker["motion_"+kind+s] for s in ("_entries","_completions","_samples")],[1,1,1])
            block=lambda event,name:next(b for b in event["blocks"] if b["name"]==name)
            self.assertEqual(block(events[1],"segment_indices")["words"],[0xffffffff])
            self.assertEqual(block(events[3],"balance_vector")["words"],[600,601,602])
            self.assertEqual(block(events[3],"balance_output")["words"],[700,701,702])
            self.assertEqual(block(events[3],"motion_prior")["words"],[800])
            self.assertNotEqual(events[3]["registers"][3],block(events[3],"balance_vector")["address"])
            self.assertEqual(len((path/"main-vehicle.jsonl").read_text().splitlines()),18)

    def test_motion_pairs_reject_invalid_operands_proofs_sequence_and_incomplete_return(self):
        for defect in ("segment-pointer","segment-caller","segment-duplicate","segment-exit-proof",
                       "segment-no-exit","segment-post-frame","segment-post-proof","tail-pointer",
                       "tail-object","tail-ac","tail-no-pre","tail-duplicate","tail-post-frame",
                       "tail-post-proof","tail-no-post"):
            with self.subTest(defect=defect):
                path,result=self.run_case("vehicle-motion-"+defect)
                self.assertNotEqual(result.returncode,0)
                self.assertFalse((path/"capture.json").exists())
                self.assertFalse((path/"main-motion.jsonl").exists())

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

    def test_vehicle_prefix_four_helpers_and_balance_preserve_saved_pointer_contracts(self):
        for mode in ("vehicle-valid","vehicle-spilled"):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode)
                self.assertEqual(result.returncode,0,result.stderr)
                marker=json.loads((path/"capture.json").read_text())
                for key in ("vehicle_calls","vehicle_completions","vehicle_samples","vehicle_prefix_entries",
                            "vehicle_prefix_completions","vehicle_prefix_samples","vehicle_balance_entries",
                            "vehicle_balance_completions","vehicle_balance_samples","vehicle_balance_calls","vehicle_balance_returns",
                            "vehicle_selection_entries","vehicle_selection_completions","vehicle_selection_samples"):
                    self.assertEqual(marker[key],1,key)
                for key in ("vehicle_slot_entries","vehicle_slot_completions","vehicle_slot_samples"):
                    self.assertEqual(marker[key],4,key)
                events=[json.loads(line) for line in (path/"main-vehicle.jsonl").read_text().splitlines()]
                self.assertEqual(marker["vehicle_events"],18)
                self.assertEqual([event["event"] for event in events],
                    ["vehicle_entry","prefix_pre","prefix_post"]+["slot_pre","slot_post"]*4+
                    ["balance_helper_pre","selection_pre","selection_post","balance_pre","balance_post","balance_helper_post","vehicle_return"])
                self.assertEqual([e["instructions"] for e in events],sorted(set(e["instructions"] for e in events)))
                self.assertEqual(len((path/"main-vehicle-cadence.jsonl").read_text().splitlines()),2)
                self.assertEqual(marker["events"],10)  # Old detailed stream is independent.
                for event in events:
                    self.assertEqual(event["parent_return_frame"],events[0]["return_frame"])
                    self.assertEqual(event["object"],0x210000)
                    self.assertEqual(len(event["object_words"]),64)
                    self.assertEqual(event["object_words"][1],0x5d3f40)
                    self.assertEqual(len(event["registers"]),32)
                    self.assertEqual(event["fp_bits"][0],1<<63)
                    self.assertEqual(event["return_frame"]["words"][2],event["return_frame"]["pc"])
                    self.assertEqual(event["return_frame"]["source"],"ordinary_ram" if mode.endswith("spilled") else "register_cache")
                prefix={b["name"]:b for b in events[2]["blocks"]}
                self.assertEqual(prefix["prefix_global"]["address"],0x213974)
                self.assertEqual(prefix["candidate_position"]["address"],0x500530)
                self.assertEqual(prefix["candidate_position"]["words"],[100,101,102])
                self.assertEqual(prefix["candidate_orientation"]["words"],[200,201,202])
                posts=[e for e in events if e["event"]=="slot_post"]
                pres=[e for e in events if e["event"]=="slot_pre"]
                self.assertEqual([e["slot_index"] for e in posts],list(range(4)))
                for i,(pre,post) in enumerate(zip(pres,posts)):
                    self.assertEqual(pre["return_frame"],post["return_frame"])
                    self.assertEqual(post["pc"],pre["return_frame"]["pc"])
                    self.assertNotEqual(pre["registers"][2],pre["return_frame"]["pc"])
                    self.assertEqual(post["registers"][31],pre["return_frame"]["frame_pointer"])
                    blocks={b["name"]:b for b in post["blocks"]}
                    for name,reg in (("slot_vector",17),("slot_triple",18),("slot_direction",19),("slot_output",21)):
                        self.assertEqual(blocks[name]["address"],pre["registers"][reg])
                        self.assertEqual(len(blocks[name]["words"]),3)
                    self.assertEqual(blocks["slot_output"]["words"],[300+10*i+j for j in range(3)])
                    self.assertNotEqual(blocks["slot_output"]["address"],post["registers"][21])
                    self.assertEqual((blocks["slot_result"]["address"],len(blocks["slot_result"]["words"])),(0x2138d0+36*i,9))
                    self.assertEqual((blocks["slot_globals"]["address"],len(blocks["slot_globals"]["words"])),(0x213880,16))
                balances=[e for e in events if e["event"].startswith("balance_")]
                self.assertEqual(balances[2]["registers"][5],0x3f000001)
                self.assertEqual(balances[2]["registers"][13:15],[0xbe000002,0xbe000001])
                blocks={b["name"]:b for b in balances[-1]["blocks"]}
                self.assertEqual((blocks["balance_pairs"]["address"],len(blocks["balance_pairs"]["words"])),(0x210064,4))
                self.assertEqual(blocks["balance_output"]["address"],balances[0]["registers"][18])
                self.assertEqual(blocks["balance_output"]["words"],[400,401,402])
                self.assertEqual(events[-1]["pc"],events[0]["return_frame"]["pc"])
                selected=next(e for e in events if e["event"]=="selection_post")
                self.assertEqual(selected["registers"][10],6)
                block=next(b for b in selected["blocks"] if b["name"]=="selected_parameters")
                self.assertEqual((block["address"],block["words"]),(0x2141c0,list(range(500,513))))

    def test_vehicle_unsampled_callbacks_are_paired_without_extra_detailed_reads(self):
        path,result=self.run_case("vehicle-unselected")
        self.assertEqual(result.returncode,0,result.stderr)
        marker=json.loads((path/"capture.json").read_text())
        self.assertEqual((marker["vehicle_calls"],marker["vehicle_completions"]),(1,1))
        self.assertEqual(marker["vehicle_slot_completions"],4)
        self.assertEqual(marker["vehicle_samples"],0)
        self.assertEqual(marker["vehicle_slot_samples"],0)
        self.assertEqual((path/"main-vehicle.jsonl").read_bytes(),b"")
        self.assertEqual(len((path/"main-vehicle-cadence.jsonl").read_text().splitlines()),2)

    def test_vehicle_rejects_unpaired_cross_frame_non_ram_or_changed_saved_caller(self):
        for suffix in ("bad-object","bad-frame","bad-caller","nested","timeout","prefix-object","prefix-ac",
                       "prefix-parent","prefix-no-pre","prefix-duplicate","prefix-no-post","slot-index",
                       "slot-pointer","slot-zero","slot-parent","slot-return","slot-nested","slot-exit-frame",
                       "slot-exit-proof","slot-no-exit","slot-return-frame","slot-no-post","balance-pointer",
                       "balance-parent","balance-nested","balance-object","balance-no-pre","balance-post-frame",
                       "balance-post-proof","balance-no-post","balance-no-exit","balance-return-frame",
                       "balance-no-return","selection-no-pre","selection-duplicate","selection-post-frame",
                       "selection-post-proof","selection-no-post","exit-proof","no-exit","return-frame","incomplete"):
            with self.subTest(suffix=suffix):
                path,result=self.run_case("vehicle-"+suffix)
                self.assertNotEqual(result.returncode,0,result.stdout)
                self.assertFalse((path/"capture.json").exists())
                self.assertFalse((path/"main-vehicle.jsonl").exists())

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
                self.assertEqual(len(events),43+4*crosses+(2 if crosses else 0))
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
                self.assertEqual(len(events),61 if sampled else 0)
                if sampled: self.assertTrue(all(event["frame"]==2571 for event in events))

    def test_main_metric_normalization_and_aggregate_snapshots_are_ordered_and_raw(self):
        for mode in ("main-valid","main-spilled","main-no-cross","main-no-normalization"):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode,"new-boundaries-"+mode)
                self.assertEqual(result.returncode,0,result.stderr)
                marker=json.loads((path/"capture.json").read_text())
                events=[json.loads(line) for line in (path/"main-contact.jsonl").read_text().splitlines()]
                crosses=0 if mode=="main-no-cross" else 4
                normalizations=0 if mode=="main-no-normalization" else 4
                for prefix,count in (("main_metric",crosses),("main_step",crosses),("main_normalization",normalizations)):
                    self.assertEqual([marker[prefix+suffix] for suffix in ("_entries","_completions","_sampled_pairs")],[count]*3)
                self.assertEqual([marker["main_aggregate"+suffix] for suffix in ("_entries","_completions","_samples")],
                                 [int(bool(crosses))]*3)
                inputs=[e for e in events if e["event"]=="cross_post"]
                metrics=[e for e in events if e["event"]=="metric_post"]
                steps=[e for e in events if e["event"]=="step_post"]
                self.assertEqual([e["metric_index"] for e in inputs],list(range(crosses)))
                self.assertEqual([e["metric_index"] for e in metrics],list(range(crosses)))
                self.assertEqual([e["step_index"] for e in metrics],list(range(crosses)))
                self.assertEqual([e["step_index"] for e in steps],list(range(crosses)))
                for index,(pre,post,step) in enumerate(zip(inputs,metrics,steps)):
                    self.assertEqual([pre["pc"],post["pc"],step["pc"]],[0x5d3b04,0x5d3b5c,0x5d3b84])
                    self.assertLess(pre["instructions"],post["instructions"])
                    self.assertLess(post["instructions"],step["instructions"])
                    for event in (pre,post,step):
                        block=next(b for b in event["blocks"] if b["name"]=="metric_cross")
                        self.assertEqual(block["address"],0x5004d0)
                        self.assertEqual(block["words"],list(range(5000+100*index,5003+100*index)))
                        self.assertEqual(event["registers"][30],0)
                        self.assertEqual(event["registers"][23],12*index)
                        self.assertEqual(event["return_frame"],events[0]["return_frame"])
                    self.assertEqual(post["registers"][28],7000+index)
                    self.assertEqual(post["registers"][21],5001+100*index)
                    self.assertEqual(step["registers"][11],8000+index)
                normal=[e for e in events if e["event"].startswith("normalization_")]
                self.assertEqual(len(normal),2*normalizations)
                for index in range(normalizations):
                    pre,post=normal[2*index:2*index+2]
                    self.assertEqual([pre["pc"],post["pc"]],[0x5d38ec,0x5d3968])
                    self.assertEqual([pre["contact_index"],post["contact_index"]],[index,index])
                    for event,base in ((pre,6000),(post,6100)):
                        block=next(b for b in event["blocks"] if b["name"]=="normalization_vector")
                        self.assertEqual(block["words"],list(range(base+100*index,base+100*index+3)))
                        self.assertEqual(block["address"],event["registers"][7])
                        self.assertEqual(block["address"]+8,event["registers"][25])
                aggregates=[e for e in events if e["event"].startswith("aggregate_")]
                if crosses:
                    self.assertEqual([e["pc"] for e in aggregates],[0x5d3b90,0x5d3cc0])
                    self.assertEqual(aggregates[-1]["object_words"][0x2c//4],9000)
                    blocks={b["name"]:b for b in aggregates[-1]["blocks"]}
                    self.assertEqual(blocks["candidate_position"]["words"][0],9001)
                    self.assertEqual(blocks["candidate_orientation"]["words"][1],9002)
                    self.assertEqual(blocks["feedback"]["words"][1:],[9003,9004])
                else:self.assertEqual(aggregates,[])

    def test_main_new_component_failures_cannot_seal_capture(self):
        for suffix in ("normalization-pointer","normalization-index","normalization-frame","normalization-parent",
                       "normalization-no-pre","normalization-duplicate-pre","normalization-no-post","normalization-repeat",
                       "metric-nonzero","metric-index","metric-frame","metric-parent","metric-no-post","metric-duplicate-post",
                       "step-index","step-object","step-no-post","step-duplicate-post",
                       "aggregate-frame","aggregate-no-pre","aggregate-duplicate-pre","aggregate-no-post","aggregate-duplicate-post"):
            with self.subTest(suffix=suffix):
                path,result=self.run_case("main-"+suffix)
                self.assertNotEqual(result.returncode,0,result.stdout)
                self.assertFalse((path/"capture.json").exists())
                self.assertFalse((path/"main-contact.jsonl").exists())

    def test_all_contact_responses_have_distinct_pre_and_post_records(self):
        for mode in ("main-valid","main-spilled","main-no-normalization"):
            with self.subTest(mode=mode):
                path,result=self.run_case(mode,"force-"+mode)
                self.assertEqual(result.returncode,0,result.stderr)
                marker=json.loads((path/"capture.json").read_text())
                self.assertEqual([marker["main_response"+suffix] for suffix in
                                  ("_entries","_completions","_sampled_pairs")],[4,4,4])
                events=[json.loads(line) for line in (path/"main-contact.jsonl").read_text().splitlines()]
                instructions=[event["instructions"] for event in events]
                self.assertEqual(instructions,sorted(set(instructions)))
                for event in events[:2]:
                    points=next(b for b in event["blocks"] if b["name"]=="local_contact_points")
                    self.assertEqual(points["address"],0x5d2e30)
                    self.assertEqual(points["words"],[(0x5d2e30+4*i)^0x12345678 for i in range(12)])
                pres=[event for event in events if event["event"]=="response_pre"]
                posts=[event for event in events if event["event"]=="response_post"]
                for index,(pre,post) in enumerate(zip(pres,posts)):
                    self.assertEqual((pre["contact_index"],post["contact_index"]),(index,index))
                    self.assertEqual((pre["pc"],post["pc"]),(0x5d372c,0x5d3a30))
                    self.assertEqual((pre["registers"][12],post["registers"][12]),(10000+index,11000+index))
                    for reg,value in ((3,0x2138ec+36*index),(9,0x2138d0+36*index),(10,0x2138e8+36*index),
                                      (5,index),(6,12*index),(15,36*index),(7,0x5004d0),(14,0x5004e0),(25,0x5004d8)):
                        self.assertEqual(pre["registers"][reg],value)
                    blocks={b["name"]:b for b in pre["blocks"]}
                    self.assertEqual(pre["registers"][20],blocks["result_slots"]["words"][9*index+7]&255)
                    self.assertEqual(len(blocks["response_vectors"]["words"]),12)
                    self.assertEqual(pre["return_frame"],events[0]["return_frame"])
                    self.assertLess(pre["instructions"],post["instructions"])
                    before=events[events.index(pre)-1]
                    self.assertEqual(before["event"],"query_post" if index<3 else "contact_post")

    def test_unpaired_or_mismatched_response_inputs_cannot_seal_capture(self):
        for suffix in ("response-pre-pointer","response-pre-offset","response-pre-frame",
                       "response-pre-object","response-pre-status","response-pre-scratch",
                       "response-pre-parent","response-no-pre","response-duplicate-pre","response-post-parent"):
            with self.subTest(suffix=suffix):
                path,result=self.run_case("main-"+suffix)
                self.assertNotEqual(result.returncode,0,result.stdout)
                self.assertFalse((path/"capture.json").exists())
                self.assertFalse((path/"main-contact.jsonl").exists())

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
