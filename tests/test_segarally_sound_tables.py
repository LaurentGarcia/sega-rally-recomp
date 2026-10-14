#!/usr/bin/env python3
"""Synthetic semantic mutations for bounded 68000 dispatch proofs."""
import json
import copy
import hashlib
import os
from pathlib import Path
import shlex
import struct
import subprocess
import tempfile
import unittest
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from segarally_sound_recompile import table_candidates
class SoundTableProofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT/'build/validation').mkdir(parents=True,exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(dir=ROOT/'build/validation');cls.path=Path(cls.temp.name)
        cls.tool=cls.path/'sound-tables'
        subprocess.run(shlex.split(os.environ.get('CXX','c++'))+['-std=c++20','-O1','-Wall','-Wextra','-Werror','-Isrc',
            'tools/srally_sound_tables/main.cpp','src/m68k/decode.cpp','src/m68k/reach.cpp','-o',str(cls.tool)],cwd=ROOT,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def image(self,low=False,long_mask=False):
        image=bytearray(0x80000)
        struct.pack_into('>4I',image,0,0x5000,0x600100,0x600100,0x600100)
        body=[0x0280,0,0x0f if low else 0xf0] if long_mask else [0x0240,0x0f if low else 0xf0]
        body += [0xe508 if low else 0xe408,0x41fb,0x0004,0x4ed0]
        struct.pack_into('>'+str(len(body))+'H',image,0x100,*body)
        end=0x100+2*len(body);jump=0x600000+end-2
        for i in range(16):
            slot=end+4*i;struct.pack_into('>2H',image,slot,0x4efa,0x300-slot-2)
        struct.pack_into('>H',image,0x300,0x4e75)
        return image,jump,end
    def run_proof(self,image,jump,seeds=''):
        program=self.path/'program.bin';program.write_bytes(image)
        seedfile=self.path/'seeds.txt';seedfile.write_text(seeds)
        return subprocess.run([str(self.tool),str(program),'600100',f'{jump:x}',str(seedfile)],cwd=ROOT,text=True,capture_output=True)
    def test_upper_lower_and_long_masks(self):
        for low,long_mask in [(False,False),(True,False),(False,True),(True,True)]:
            image,jump,base=self.image(low,long_mask);run=self.run_proof(image,jump)
            self.assertEqual(run.returncode,0,run.stderr);proof=json.loads(run.stdout)
            self.assertEqual((proof['table_base'],proof['entries'],proof['stride']),(0x600000+base,16,4))
            self.assertEqual([r['address']for r in proof['candidates']],[0x600000+base+4*i for i in range(16)])
            self.assertEqual({r['direct_target']for r in proof['candidates']},{0x600300})
    def test_semantic_mutations_rejected(self):
        # Every mutation still supplies a readable image: the decoder/operand proof must reject it.
        changes={
          'mask':(0x102,0xff),'byte_mask_keeps_unknown_word_bits':(0x100,0x0200),
          'wrong_shift_direction':(0x104,0xe508),'wrong_shift_count':(0x104,0xe608),
          'wrong_shift_register':(0x104,0xe409),'wide_shift_keeps_unknown_upper_bits':(0x104,0xe488),
          'wrong_index_register':(0x108,0x1004),'long_index_has_unknown_upper_bits':(0x108,0x0804),
          'wrong_jump_register':(0x10a,0x4ed1),'unsupported_slot':(0x10c,0x4e71),
          'odd_jump_target':(0x10e,0x1f3),'out_of_rom_jump_target':(0x10e,0x8000)}
        for label,(offset,word) in changes.items():
            with self.subTest(label=label):
                image,jump,_=self.image();struct.pack_into('>H',image,offset,word)
                self.assertNotEqual(self.run_proof(image,jump).returncode,0,label)
    def test_known_seed_and_branch_bypasses_rejected(self):
        image,jump,_=self.image()
        self.assertNotEqual(self.run_proof(image,jump,'600104\n').returncode,0)
        struct.pack_into('>2H',image,0x400,0x6000,0x600104-(0x600400+2)&0xffff)
        self.assertNotEqual(self.run_proof(image,jump,'600400\n').returncode,0)
    def test_body_must_be_reachable_and_complete(self):
        image,jump,_=self.image();struct.pack_into('>3I',image,4,0x600300,0x600300,0x600300)
        self.assertNotEqual(self.run_proof(image,jump).returncode,0)
        image,jump,_=self.image();self.assertNotEqual(self.run_proof(image,jump+2).returncode,0)
    def test_pipeline_hashes_and_dimensions(self):
        image,jump,base=self.image()
        program=self.path/'audited.bin';program.write_bytes(image)
        seeds=self.path/'audited-seeds.txt';seeds.write_text('')
        config={'schema':1,'romset':'srallycb','board':'model2a','program_sha256':hashlib.sha256(image).hexdigest(),
            'tables':[{'name':'synthetic','kind':'nibble_pc_relative_jumps','entries':16,'stride':4,
                'start':'0x600100','jump':hex(jump),'table_base':hex(0x600000+base),'body_size':base-0x100,
                'body_sha256':hashlib.sha256(image[0x100:base]).hexdigest(),
                'table_sha256':hashlib.sha256(image[base:base+64]).hexdigest()}]}
        rows=table_candidates(image,config,self.tool,seeds,program)
        self.assertEqual(len(rows),16)
        self.assertTrue(all(row['observed'] is False for row in rows))
        for key,value in [('program_sha256','0'*64),('board','model2b'),('schema',2)]:
            bad=copy.deepcopy(config);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):table_candidates(image,bad,self.tool,seeds,program)
        for key,value in [('body_sha256','0'*64),('table_sha256','0'*64),('entries',32),('stride',8),('body_size',14)]:
            bad=copy.deepcopy(config);bad['tables'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):table_candidates(image,bad,self.tool,seeds,program)
        for offset in [0x102,base+2]:
            changed=bytearray(image);changed[offset]^=1
            bad=copy.deepcopy(config);bad['program_sha256']=hashlib.sha256(changed).hexdigest()
            program.write_bytes(changed)
            with self.subTest(offset=offset),self.assertRaises(ValueError):table_candidates(changed,bad,self.tool,seeds,program)
if __name__=='__main__':unittest.main()
