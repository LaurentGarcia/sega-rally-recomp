#!/usr/bin/env python3
"""ROM-free rejection tests for measured host build binding; no compiler runs."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location("recording_reference_build",Path(__file__).with_name("test_segarally_recording_reference.py"))
build=importlib.util.module_from_spec(spec);spec.loader.exec_module(build)


class BindingTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();self.host=self.root/"host";self.current=self.root/"current";self.out=self.root/"build";self.evidence=self.root/"evidence"
        self.saved_root,self.saved_runtime=build.ROOT,build.RUNTIME
        build.ROOT=self.current;build.RUNTIME=["model2a_machine"]
        self.addCleanup(self.restore)
        self.source=self.host/"src/runtime/model2a_machine.cpp"
        self.put(self.source,"synthetic runtime source\n");self.put(self.current/"src/runtime/model2a_machine.cpp",self.source.read_text())
        self.obj=self.out/"machine.o";self.put(self.obj,"synthetic compiled object\n")
        self.put(self.out/"segarally_native","synthetic measured executable\n")
        for label,target in (("native","segarally_native"),("recording","segarally_recording")):
            text="c++ machine.o -o segarally_native\n" if label=="native" else "ar qc libsegarally_recording.a\n"
            self.put(self.out/"CMakeFiles"/(target+".dir")/"link.txt",text);self.put(self.evidence/(label+"-link.txt"),text)
        command={"file":str(self.source),"directory":str(self.out),"command":"c++ -c source -o machine.o"}
        proof={"source":str(self.source),"directory":str(self.out),"command":command["command"],"object":"machine.o","source_identity":build.identity(self.source),"object_identity":build.identity(self.obj)}
        self.put_json(self.out/"compile_commands.json",[command]);self.put_json(self.evidence/"compiled-object-contract.json",[proof])
        self.put_json(self.evidence/"linked-artifact-identities.json",{"segarally_native":build.identity(self.out/"segarally_native")})

    def restore(self):build.ROOT,build.RUNTIME=self.saved_root,self.saved_runtime
    def put(self,path,text):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)
    def put_json(self,path,value):self.put(path,json.dumps(value))
    def verify(self):return build.verify_host(self.out,self.evidence)

    def test_actual_measurements(self):self.assertEqual(self.verify()[0],self.host)
    def test_changed_executable(self):
        self.put(self.out/"segarally_native","different");self.assertRaises(ValueError,self.verify)
    def test_changed_object(self):
        self.put(self.obj,"different");self.assertRaises(ValueError,self.verify)
    def test_changed_measured_source(self):
        self.put(self.source,"different");self.assertRaises(ValueError,self.verify)
    def test_changed_consumer_runtime(self):
        self.put(self.current/"src/runtime/model2a_machine.cpp","different");self.assertRaises(ValueError,self.verify)
    def test_changed_link(self):
        self.put(self.out/"CMakeFiles/segarally_native.dir/link.txt","c++ unmeasured.o");self.assertRaises(ValueError,self.verify)
    def test_duplicate_keys_and_bounds(self):
        self.put(self.root/"bad.json",'{"value":1,"value":2}');self.assertRaises(ValueError,build.read_json,self.root/"bad.json")
        self.assertRaises(ValueError,build.identity,self.obj,1)


if __name__=="__main__":unittest.main()
