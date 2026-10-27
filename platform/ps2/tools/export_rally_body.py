#!/usr/bin/env python3
"""Privately export one verified raw model, excluding captured view matrices."""
import argparse
import json
from pathlib import Path
import struct
import zlib

from check_contact_neighborhood import ROOT, digest, private_output
from export_native_mesh import export_capture


def body_from_verified_mesh(data):
    if len(data)<64 or data[:8]!=b"RLYMESH\0":
        raise ValueError("Invalid verified mesh")
    version,header,vertices,faces,objects,crc,size,vs,fs,os=struct.unpack_from("<10I",data,8)
    if (version,header,objects,vs,fs,os)!=(1,64,1,12,16,80) or \
       not 3<=vertices<=65536 or not 1<=faces<=16384 or size!=vertices*12+faces*16+80 or \
       len(data)!=64+size or zlib.crc32(data[64:])!=crc:
        raise ValueError("Verified body mesh size/format mismatch")
    payload=data[64:64+vertices*12+faces*16]
    # Matrices are deliberately not copied into the native body asset.
    return struct.pack("<8s8I24x",b"RLYBODY\0",1,64,vertices,faces,len(payload),zlib.crc32(payload),12,16)+payload


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture",type=Path,required=True)
    parser.add_argument("--polygons",type=Path,required=True)
    parser.add_argument("--object",type=int,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    try:
        if not 0<=args.object<1024:raise ValueError("Object index out of range")
        output=private_output(args.output)
        output.mkdir(parents=True,exist_ok=False)
        verified=output/"verified-input.bin"
        provenance=export_capture(args.capture,args.polygons,verified,{args.object})
        body=body_from_verified_mesh(verified.read_bytes())
        target=output/"body.bin";target.write_bytes(body)
        report={"schema":1,"status":"passed","capture_object":args.object,
            "body_sha256":digest(target),"body_bytes":len(body),"verified_mesh_sha256":digest(verified),
            "source":provenance,"reference_view_matrix_copied":False,
            "model_to_car_bridge_proven":False,"native_gameplay_connected":False,
            "sources_sha256":{name:digest(ROOT/name) for name in (
                "platform/ps2/tools/export_rally_body.py","platform/ps2/tools/export_native_mesh.py",
                "platform/ps2/tools/check_contact_neighborhood.py")}}
        (output/"result.json").write_text(json.dumps(report,indent=2)+"\n")
        print(f"RALLY_BODY EXPORT vertices={provenance['vertices']} faces={provenance['faces']} bytes={len(body)}")
    except (ValueError,OSError,KeyError,TypeError) as error:parser.error(str(error))


if __name__=="__main__":main()
