"""Original-stage extraction rejects incomplete or repointed execution witnesses."""
import io
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"platform/ps2/tools"))
import extract_main_stages as stages
from full_step_capture import offset


class MainStageExtractionTests(unittest.TestCase):
    def run_case(self,defect=None):
        memory=bytearray(0x140000);fp=0x200980;obj=0x500000
        parent={"event":"vehicle_entry","object":obj,"vehicle_frame_pointer":fp,"instructions":10,
                "frame":100,"blocks":[{"name":"object","address":obj,"words":[0]*64}]}
        def replay(*args,on_boundary,on_transaction,**kwargs):
            on_boundary(parent,memoryview(memory).toreadonly())
            if defect!="entry-end": on_transaction((20,0x5d3ff0,0x5d3ff0,0,4),memoryview(memory).toreadonly())
            if defect!="front-start": on_transaction((30,0x5d4224,0x5d4224,0,4),memoryview(memory).toreadonly())
            address=fp+0x1a0+(4 if defect=="scalar-address" else 0)
            if defect!="scalar-missing":
                on_transaction((40,0x5d46bc,address,0x3f800000,0x104),memoryview(memory).toreadonly())
                struct.pack_into("<I",memory,offset(address,4),0x3f800000)
            if defect=="scalar-duplicate": on_transaction((41,0x5d46bc,address,0x3f800000,0x104),memoryview(memory).toreadonly())
            if defect!="helper-end": on_boundary(dict(parent,event="balance_helper_pre",instructions=50),memoryview(memory).toreadonly())
            yield {"frame":100}
        entry,front=io.BytesIO(),io.BytesIO()
        paths={"full-step-seed.bin":None,"full-step-access.bin":None}
        with patch.object(stages,"replay_steps",replay): proof=stages.extract([[]],{},paths,entry,front)
        return proof,entry.getvalue(),front.getvalue()

    def test_exact_original_boundary_inputs_have_distinct_formats_and_scalar_word(self):
        proof,entry,front=self.run_case()
        self.assertEqual(entry[:12],b"ENTSTEP1\x01\x00\x00\x00")
        self.assertEqual(front[:12],b"FRONTST1\x01\x00\x00\x00")
        self.assertEqual(entry[-4:],bytes(4));self.assertEqual(front[-4:],struct.pack("<I",0x3f800000))
        self.assertEqual(proof[0]["entry_end"],20);self.assertEqual(proof[0]["front_start"],30)
        for data in (entry,front):
            frame,fp,obj,count=struct.unpack_from("<4I",data,12)
            self.assertEqual((frame,fp,obj),(100,0x200980,0x500000));cursor=28;addresses=set()
            for _ in range(count):
                address,n=struct.unpack_from("<2I",data,cursor);cursor+=8
                addresses.update(range(address,address+4*n,4));cursor+=8*n
            self.assertEqual(cursor+4,len(data))
            self.assertTrue({0x500000,0x5000fc,0x200980,0x200b70}<=addresses)

    def test_missing_or_coherently_repointed_stage_evidence_is_rejected(self):
        for defect in ("entry-end","front-start","scalar-address","scalar-missing","scalar-duplicate","helper-end"):
            with self.subTest(defect=defect),self.assertRaises(ValueError): self.run_case(defect)


if __name__=="__main__": unittest.main()
