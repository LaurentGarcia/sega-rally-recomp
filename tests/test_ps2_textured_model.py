import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'platform/ps2/tools'))
import export_textured_model as exporter

class TexturedModelTests(unittest.TestCase):
    def test_loader_rejection_and_replacement(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++20 compiler unavailable')
        with tempfile.TemporaryDirectory()as tmp:
            exe=Path(tmp)/'loader'
            subprocess.run([compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off',
                '-I'+str(ROOT/'platform/ps2/include'),str(ROOT/'platform/ps2/tests/native_textured_model_test.cpp'),
                str(ROOT/'platform/ps2/src/native_textured_model.cpp'),'-o',str(exe)],check=True,capture_output=True,text=True)
            asset=Path(tmp)/'synthetic.bin'
            asset.write_bytes(exporter.encode([(0,0,0),(1,0,0),(0,1,0)], [([0,1,2],[0,0,8,0,0,8],0)],
                [(0,0xffffffff,3,(0xff102030,)*15+(0,))],[(8,8,bytes(range(32)))]))
            result=subprocess.run([str(exe),str(asset)],check=True,capture_output=True,text=True)
            self.assertIn('TEXTURED_MODEL PASS checks=',result.stdout)
            self.assertIn('FILE vertices=3 faces=1 materials=1',result.stdout)

    def test_native_index_order_mirrored_period_and_palette(self):
        memory={'palette':bytes(0x4000),'translation':b''.join(struct.pack('<H',min(64+i%256,255))for i in range(0x6000)),
            'luma':bytes([16])*0x20000,'texture0':struct.pack('<I',0x01234567)*0x80000,'texture1':bytes(0x200000)}
        cache={};textures=[]
        material=exporter.decode_material([0x6000,0,0,0],128,memory,cache,textures)
        self.assertEqual(textures[0][0:2],(32,32));self.assertEqual(textures[0][2][:16],bytes([0x54,0x10])*8)
        self.assertEqual(textures[0][2][16:32],bytes([0x76,0x32])*8)
        expected=exporter.gamma(72);self.assertEqual(material[3][0],0xff000000|expected*0x010101)
        self.assertEqual(material[3][15],0)
        mirrored=exporter.decode_material([0x6300,0,0,0],128,memory,cache,textures)
        self.assertEqual(textures[1][0:2],(64,64));self.assertEqual(textures[1][2][16:32],bytes([0x01,0x45])*8)
        self.assertEqual(textures[1][2][-32:-16],bytes([0x54,0x10])*8)
        self.assertEqual(mirrored[0],1)
        exporter.decode_material([0x6300,0,0,0],64,memory,cache,textures)
        self.assertEqual(len(textures),2)
        self.assertIsNone(exporter.decode_material([0x2000,0,0,0],128,memory,cache,textures))
        for header in ([0x5000,0,0,0],[0x4004,0,0,0]):
            with self.assertRaises(ValueError):exporter.decode_material(header,128,memory,cache,textures)

    def test_authored_facing_preserves_normal_anchor_and_translation_rule(self):
        def bits(x):return struct.unpack('<I',struct.pack('<f',x))[0]
        record={'mode':1,'points':[[bits(v)for v in p]for p in((0,0,0),(1,0,0),(2,3,4),(1,1,0))]}
        words=[0]*6+[0x601]+[bits(v)for v in(1,2,3)]+[0]*6
        raw=struct.pack('<16I',*words)
        facing=exporter.source_facing(record,raw,0,[1,0,2,3])
        self.assertEqual(facing,(1,2,3,2,3,4,1))
        # Rotation and translation are deliberately different: a vector must
        # not inherit the point's translation. Authoring is independent of winding.
        matrix=[0,0,-1,0,0,1,0,0,1,0,0,0,10,20,30,1]
        self.assertEqual(exporter.source_facing(record,raw,0,[1,0,2,3],matrix),(3,2,-1,14,23,28,1))
        words[6]|=1<<17
        self.assertEqual(exporter.source_facing(record,struct.pack('<16I',*words),0,[1,0,2,3]),(0,0,0,0,0,0,0))
        record['mode']=3
        with self.assertRaises(ValueError):exporter.source_facing(record,raw,0,[1,0,2,3])

    def test_v2_keeps_opposite_authored_normals_without_uv_flip(self):
        vertices=[(0,0,1),(1,0,1),(0,1,1)]
        uv=[0,0,8,0,0,8]
        faces=[([0,1,2],uv,0,(0,0,1,0,0,1,1)),([0,1,2],uv,0,(0,0,-1,0,0,1,1))]
        blob=exporter.encode(vertices,faces,[(0xffffffff,0xffffffff,0,(0,)*16)],[])
        self.assertEqual(blob[:8],b'RLYTEXM2');self.assertEqual(struct.unpack_from('<I',blob,48)[0],68)
        first=64+36
        self.assertEqual(blob[first:first+40],blob[first+68:first+108])
        self.assertEqual(struct.unpack_from('<6fI',blob,first+40),(0,0,1,0,0,1,1))
        self.assertEqual(struct.unpack_from('<6fI',blob,first+108),(0,0,-1,0,0,1,1))
        with self.assertRaises(ValueError):exporter.encode(vertices,[([0,1,2],uv,0,(0,0,1,0,0,1,2))],[(0xffffffff,0xffffffff,0,(0,)*16)],[])

    def test_link_corner_material_order(self):
        words=[0]*6+[0x203]+[0]*9+[0x102]+[0]*9
        record={'links':2,'points':[[0,0,0]]*5}
        self.assertEqual(exporter.link_faces(record,struct.pack('<26I',*words)),[(0x203,[1,0,2,3]),(0x102,[3,2,4])])

if __name__=='__main__':unittest.main()
