#!/usr/bin/env python3
"""Export privately observed raw geometry/materials to a bounded native model.

Texture addressing/shading follows the project's BSD-licensed model2_gs.cpp
reference. No original bytes or camera transforms are embedded in this tool.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib
from export_native_mesh import ROOT, POLYGON_SHA256, verify_decoded_source, scalar

def digest(data):return hashlib.sha256(data).hexdigest()
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def word(data,index):return struct.unpack_from('<H',data,index*2)[0]
def gamma(x):return int(max((x-64)*255/191,0))

def decode_material(header,luma,memory,texture_cache,textures):
    if len(header)!=4 or any(type(x)is not int or not 0<=x<65536 for x in header) or type(luma)is not int or not 0<=luma<=255:raise ValueError('invalid material header/luma')
    th0,th1,th2,th3=header;renderer=th0>>13&3
    if renderer==1:return None
    color=word(memory['palette'],(th3>>6&1023)+0x1000)&0x7fff
    def rgb(level):
        channels=[gamma(word(memory['translation'],base+((color>>shift&31)<<8)+level)&255) for base,shift in ((0,0),(0x2000,5),(0x4000,10))]
        return 0xff000000|channels[0]<<16|channels[1]<<8|channels[2]
    flags=(1 if renderer==3 else 0)|(2 if th0&0x8000 else 0)
    if renderer==0:return (~0&0xffffffff,rgb(luma>>2),flags,(0,)*16)
    if th0&0x1000:raise ValueError('microtexture is outside native material support')
    w,h=32<<(th0&7),32<<(th0>>3&7);mx,my=bool(th0&256),bool(th0&512)
    uw,uh=w*(2 if mx else 1),h*(2 if my else 1)
    if uw>256 or uh>256:raise ValueError('texture/mirrored period exceeds256')
    x,y=(th2&63)*32,(th2>>6&31)*32;sheet=th2>>12&1
    key=(sheet,x,y,w,h,mx,my)
    if key not in texture_cache:
        source=memory['texture'+str(sheet)];indices=bytearray(uw*uh//2)
        def texel(px,py):
            gx,gy=x+px,y+py
            if gx>=1024:gx-=1024;gy^=1024
            offset=(gy//2)*512+gx//2
            v=struct.unpack_from('<I',source,((offset>>1)&0x7ffff)*4)[0]
            if offset&1:v>>=16
            if not py&1:v>>=8
            if not px&1:v>>=4
            return v&15
        for row in range(uh):
            sy=row if row<h else 2*h-1-row
            for col in range(0,uw,2):
                sx0=col if col<w else 2*w-1-col
                sx1=col+1 if col+1<w else 2*w-2-col
                indices[(row*uw+col)//2]=texel(sx0,sy)|(texel(sx1,sy)<<4)
        texture_cache[key]=len(textures);textures.append((uw,uh,bytes(indices)))
    lumabase=(th1&255)<<7
    palette=[rgb(min(memory['luma'][(lumabase+i*8)*4]*luma//256,63)) for i in range(16)]
    if renderer==3:palette[15]=0
    return (texture_cache[key],0xffffffff,flags,tuple(palette))

def link_faces(record,raw):
    """Associate source link ordinal with indexed corners, independently of culling."""
    words=struct.unpack('<%dI'%(len(raw)//4),raw);cursor=6;edge=[0,1];count=2;result=[]
    for link in range(record['links']):
        attr=words[cursor];a=count;count+=1;b=a
        if attr&1:b=count;count+=1
        corners=[edge[1],edge[0],a]+([b] if attr&1 else [])
        result.append((attr,corners));connection=attr>>8&3
        edge=([a,b],[edge[0],a],[a,b],[b,edge[1]])[connection];cursor+=10
    if count!=len(record['points']):raise ValueError('link/point count mismatch')
    return result

def source_facing(record,raw,ordinal,corners,matrix=None):
    """Preserve supplied normal/anchor; visibility is decided for the live view."""
    attr=struct.unpack_from('<I',raw,(6+ordinal*10)*4)[0]
    if attr&(1<<17):return (0.,0.,0.,0.,0.,0.,0)
    if (record['mode']&3)>=2:raise ValueError('single-sided computed-normal mode requires separate conversion proof')
    normal=[scalar(w)for w in struct.unpack_from('<3I',raw,(7+ordinal*10)*4)]
    point=[scalar(w)for w in record['points'][corners[2]]]
    if matrix:
        normal=transform(normal,matrix,False);point=transform(point,matrix,True)
    return (*normal,*point,1)

def transform(point,matrix,translate=True):
    identity=[1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.]
    if matrix==identity:return list(point)
    values=[f32(f32(f32(point[0]*matrix[r])+f32(point[1]*matrix[r+4]))+f32(point[2]*matrix[r+8]))for r in range(3)]
    return [f32(v+matrix[r+12])for r,v in enumerate(values)]if translate else values

def encode(vertices,faces,materials,textures):
    if not 3<=len(vertices)<=65536 or not 1<=len(faces)<=32768 or not 1<=len(materials)<=4096 or len(textures)>1024:raise ValueError('native model count limit')
    version=2 if any(len(face)==4 for face in faces)else 1
    stride=68 if version==2 else 40
    payload=bytearray()
    for v in vertices:
        if len(v)!=3:raise ValueError('invalid XYZ')
        for x in v:scalar(struct.unpack('<I',struct.pack('<f',x))[0])
        payload.extend(struct.pack('<3f',*v))
    for face in faces:
        if len(face)not in(3,4):raise ValueError('invalid native face record')
        ids,uv,material=face[:3]
        if len(set(ids))!=3 or min(ids)<0 or max(ids)>=len(vertices) or not 0<=material<len(materials):raise ValueError('invalid native face')
        if len(uv)!=6:raise ValueError('invalid native UV')
        for x in uv:scalar(struct.unpack('<I',struct.pack('<f',x))[0])
        payload.extend(struct.pack('<3I6fI',*ids,*uv,material))
        if version==2:
            facing=face[3]if len(face)==4 else(0.,)*6+(0,)
            if len(facing)!=7 or type(facing[6])is not int or facing[6]not in(0,1):raise ValueError('invalid authored facing')
            for x in facing[:6]:scalar(struct.unpack('<I',struct.pack('<f',x))[0])
            payload.extend(struct.pack('<6fI',*facing))
    for texture,color,flags,palette in materials:payload.extend(struct.pack('<19I',texture,color,flags,*palette))
    indices=bytearray()
    for w,h,data in textures:
        if w<8 or h<8 or w>256 or h>256 or w&(w-1) or h&(h-1) or len(data)!=w*h//2:raise ValueError('invalid packed texture')
        payload.extend(struct.pack('<4I',w,h,len(indices),len(data)));indices.extend(data)
    if len(indices)>4*1024*1024:raise ValueError('native index-byte limit')
    payload.extend(indices)
    header=struct.pack('<8s14I',b'RLYTEXM2'if version==2 else b'RLYTEXM1',version,64,len(vertices),len(faces),len(materials),len(textures),len(indices),zlib.crc32(payload),len(payload),12,stride,76,16,0)
    return header+payload

def verified_frame(directory,polygons):
    directory=directory.resolve();capture=directory.parent;validation=capture.parent/'validation.json'
    if directory.is_symlink() or not directory.is_relative_to((ROOT/'build').resolve()):raise ValueError('capture must stay under build')
    report=json.loads(validation.read_text())
    if report.get('status')!='passed' or report.get('exact_checkpoint_match')is not True:raise ValueError('visual capture has no passed observer parity')
    hashes=report['capture_files_sha256']
    def data(name):
        p=directory/name
        if p.is_symlink() or not p.is_file() or p.stat().st_size>16*1024*1024:raise ValueError('invalid captured file')
        b=p.read_bytes()
        if hashes.get(str(p.relative_to(capture)))!=digest(b):raise ValueError('captured file identity differs: '+name)
        return b
    summary=json.loads(data('capture.json'));visual=json.loads(data('visual.json'))
    records=[json.loads(s) for s in data('objects.jsonl').splitlines()]
    material_records=[json.loads(s) for s in data('materials.jsonl').splitlines()]
    rom=polygons.read_bytes()
    if digest(rom)!=POLYGON_SHA256:raise ValueError('unsupported polygon image')
    if not summary['complete'] or not visual['complete'] or len(records)!=summary['objects'] or len(records)!=visual['objects'] or len(material_records)!=visual['links']:raise ValueError('visual count mismatch')
    raw=[];total=0
    for i,r in enumerate(records):
        if r['schema']!=1 or r['object']!=i or r['frame']!=summary['frame'] or r['source_file']!=f'object-{i}.bin':raise ValueError('invalid model identity')
        b=data(r['source_file']);verify_decoded_source(r,b)
        if len(b)!=r['source_words']*4:raise ValueError('raw source count mismatch')
        address=r['object_address'];is_rom=bool(address&0x800000) and not address&0x1000000
        if is_rom:
            if r['source_size_words']*4!=len(rom) or r['source_start_word']!=address&(len(rom)//4-1):raise ValueError('polygon source addressing differs')
            begin=r['source_start_word']*4;expected=(rom+rom)[begin:begin+len(b)]
            if b!=expected:raise ValueError('raw source differs from original polygon ROM')
        elif r['source_size_words']!=0x8000 or r['source_start_word']!=address&0x7fff:raise ValueError('invalid source RAM range')
        raw.append(b);total+=len(b)//4
    if total!=summary['source_words']:raise ValueError('aggregate source count mismatch')
    by_object=[[] for _ in records]
    for m in material_records:
        i=m['object']
        if m['schema']!=1 or m['frame']!=summary['frame'] or not 0<=i<len(records) or m['link']!=len(by_object[i]):raise ValueError('material object/link order differs')
        by_object[i].append(m)
    for r,links in zip(records,by_object):
        if len(links)!=r['links']:raise ValueError('missing pre-cull material links')
    memory={name:data(name+'.bin') for name in ('palette','translation','luma','texture0','texture1')}
    for name,n in {'palette':0x4000,'translation':0xc000,'luma':0x20000,'texture0':0x200000,'texture1':0x200000}.items():
        if len(memory[name])!=n:raise ValueError('video memory extent mismatch')
    return records,raw,by_object,memory,{'validation_sha256':digest(validation.read_bytes()),'frame':summary['frame'],'direct_commands_excluded':summary['direct_commands_excluded']}

def export_model(directory,polygons,output,selected,bindings=None):
    output=output.resolve();build=ROOT/'build'
    if build.is_symlink() or not output.is_relative_to(build.resolve()) or output==build or output.suffix!='.bin' or output.exists() or output.with_suffix('.json').exists():raise ValueError('new .bin output beneath real build required')
    records,raw,links,memory,provenance=verified_frame(directory,polygons)
    if not selected or len(set(selected))!=len(selected) or any(type(i)is not int or not 0<=i<len(records) for i in selected):raise ValueError('invalid selected object order')
    matrices={};binding_report=None
    if bindings:
        binding_report=json.loads(bindings.read_text())
        if binding_report.get('space')not in ('world','car_local','wheel_local') or binding_report.get('proven')is not True:raise ValueError('binding must identify proven native space')
        evidence=Path(binding_report['evidence']['path']).resolve()
        if not evidence.is_relative_to(build.resolve()) or digest(evidence.read_bytes())!=binding_report['evidence']['sha256']:raise ValueError('binding evidence identity differs')
        for item in binding_report['objects']:
            if item['id'] in matrices or len(item['matrix_bits'])!=16:raise ValueError('duplicate/malformed model binding')
            m=[scalar(b) for b in item['matrix_bits']]
            if [m[3],m[7],m[11],m[15]]!=[0,0,0,1]:raise ValueError('binding is not affine')
            matrices[item['id']]=m
        if set(matrices)!=set(selected):raise ValueError('binding must cover exactly selected objects')
    elif len(selected)!=1:raise ValueError('multiple objects require proven native-space assembly bindings')
    vertices=[];faces=[];materials=[];textures=[];texture_cache={};material_cache={};source=[];skipped={'hidden':0,'degenerate':0,'renderer1':0}
    for object_id in selected:
        record=records[object_id];first=len(vertices);m=matrices.get(object_id)
        for bits in record['points']:
            v=[scalar(b) for b in bits]
            if m:v=transform(v,m)
            vertices.append(v)
        for ordinal,((attr,corners),material_record) in enumerate(zip(link_faces(record,raw[object_id]),links[object_id])):
            if material_record['attributes']!=attr&0x3ffff or material_record['corners']!=len(corners):raise ValueError('material topology/attributes differ')
            if not attr>>8&3:skipped['hidden']+=1;continue
            material=decode_material(material_record['header'],material_record['luma'],memory,texture_cache,textures)
            if material is None:skipped['renderer1']+=1;continue
            if material not in material_cache:material_cache[material]=len(materials);materials.append(material)
            uv=material_record['uv_bits']
            if len(uv)!=len(corners):raise ValueError('material UV count differs')
            uv=[[scalar(v)/8 for v in p] for p in uv]
            facing=source_facing(record,raw[object_id],ordinal,corners,m)
            for fan in ((0,1,2),(0,2,3)) if len(corners)==4 else ((0,1,2),):
                ids=[first+corners[i] for i in fan]
                if len(set(ids))<3:skipped['degenerate']+=1;continue
                faces.append((ids,[v for i in fan for v in uv[i]],material_cache[material],facing))
        source.append({'object':object_id,'source_sha256':digest(raw[object_id]),'source_address':record['object_address'],'mode':record['mode'],'matrix_bits':record['matrix_bits']})
    blob=encode(vertices,faces,materials,textures);output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('xb')as f:f.write(blob)
    report={'schema':1,'status':'passed','asset_sha256':digest(blob),'bytes':len(blob),'vertices':len(vertices),'faces':len(faces),
        'materials':len(materials),'textures':len(textures),'index_bytes':sum(len(t[2])for t in textures),'source':source,'capture':provenance,
        'binding':binding_report,'native_space':binding_report['space'] if binding_report else 'unbound_single_source_model',
        'skipped':skipped,'source_visibility_filtered':False,'authored_facing_version':2,'original_culling_attributes_retained_in_capture':True,
        'limits':['Captured per-face lighting/CLUT is fixed, not dynamic lighting recovery.','Nearest base texels; filtered borders/mipmapping differ.','Authored normal/point facing is retained; native depth remains distinct from original polygon priority.','No captured view matrix or source address is present in the native binary.'],
        'sources_sha256':{str(p.relative_to(ROOT)):digest(p.read_bytes()) for p in (Path(__file__),ROOT/'platform/ps2/tools/export_native_mesh.py')}}
    with output.with_suffix('.json').open('x')as f:json.dump(report,f,indent=2);f.write('\n')
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path,required=True);parser.add_argument('--polygons',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);parser.add_argument('--objects',required=True);parser.add_argument('--bindings',type=Path)
    args=parser.parse_args()
    try:r=export_model(args.capture,args.polygons,args.output,[int(i)for i in args.objects.split(',')],args.bindings)
    except (ValueError,OSError,KeyError,TypeError,struct.error)as e:parser.error(str(e))
    print(json.dumps({k:r[k]for k in ('status','vertices','faces','materials','textures','index_bytes','bytes','native_space')},indent=2))
if __name__=='__main__':main()
