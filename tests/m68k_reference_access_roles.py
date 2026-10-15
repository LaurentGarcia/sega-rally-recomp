"""Test-only observation purpose from pinned M68000 bus-init microcode.

PROGRAM function-code reads are not necessarily instruction words. The pinned
FC equation selects PROGRAM for an operand when ir_is_pc_rel and micro15/16
are both clear; explicit instruction-stream cycles set micro16. Propagate this
purpose along the same bus initiation/completion control-flow as upstream.
No addresses, read values, native observations or expected counts are inputs.
"""
def install(g):
 namespace=g['generate_base_code_for_microcode'].__globals__
 original_base=namespace['generate_base_code_for_microcode']
 original_propagate=namespace['propagate_bus_access']
 roles={}
 report=[]
 def base(ir,irmask,madr,tvn,group01):
  code=original_base(ir,irmask,madr,tvn,group01)
  micro=namespace['microcode_table'][madr]
  pc_relative=(ir&0x003e)==0x003a and ((ir&0xf000)!=0xe000 or (ir&0x00c0)==0x00c0)
  micro15=bool(micro&0x08000);micro16=bool(micro&0x10000)
  for ci in code:
   if ci[0]!='bus':continue
   fc=2 if (((not pc_relative) and not micro16) or micro15) and ((pc_relative and not micro15) or micro16) else 0 if ((pc_relative and not micro15) or micro16) else 1 if (((not pc_relative) and not micro16) or micro15) else 3
   if ci[1]!=fc or fc not in (0,1,2):raise ValueError('unknown pinned FC bus profile')
   role='o' if fc==0 and pc_relative and not micro15 and not micro16 else 'p' if fc==0 else 'd' if fc==1 else 'i'
   roles[id(ci)]=(role,fc,ir,madr)
  return code
 def propagate(blocks,code,critical,mode):
  original_propagate(blocks,code,critical,mode)
  seen=set();annotations={}
  def visit(index,active):
   key=(index,None if active is None else active[:2])
   if key in seen:return
   seen.add(key)
   for offset,ci in enumerate(code[index]):
    if ci[0]=='bus':active=roles[id(ci)]
    elif ci[0]=='bus_end':
     if active is None or len(ci)!=11 or ci[2]!=active[1]:raise ValueError('unproven observer bus completion')
     role,fc,ir,madr=active
     # IRC destination is evidence, not the purpose classifier: long immediate
     # and absolute extensions can load DBIN without latching IRC.
     latch=offset+1<len(code[index]) and code[index][offset+1]==['=',namespace['R'].irc,namespace['R'].edb]
     if id(ci) in annotations and annotations[id(ci)]!=role:raise ValueError('ambiguous observer purpose at bus completion')
     first=id(ci) not in annotations
     annotations[id(ci)]=role
     if first:report.append({'opcode':ir,'bus_start_microaddress':madr,'FC':fc,'role':role,'write':bool(ci[4]),'latches_IRC':latch})
     active=None
   for successor in blocks[index][3]:visit(successor,active)
  visit(0,None)
  for block in code:
   revised=[]
   for ci in block:
    if ci[0]=='bus_end':
     if id(ci) not in annotations:raise ValueError('unreachable/unclassified bus completion')
     revised.append(['i',"observer_access_role = '%s';"%annotations[id(ci)]])
    revised.append(ci)
   block[:]=revised
 namespace['generate_base_code_for_microcode']=base
 namespace['propagate_bus_access']=propagate
 return report
