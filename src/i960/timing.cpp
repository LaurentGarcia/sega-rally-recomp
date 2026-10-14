// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
// Fixed m_icount deductions adapted from MAME src/devices/cpu/i960/i960.cpp
// commit dddd73680656e355bb2b5beecab1167c9f07bf81, SHA256
// f7d887add0cb070f91f585b22f3b35acc4fc94ab5c94a527eab5205ee9440fdd.
// Device framework/semantics removed; deductions become compiler metadata.
// Source describes remr as 67..75878 real cycles but deducts fixed 67;
// expr deducts 334 ('checkme'). Preserve these reference limitations.
#include "timing.h"
namespace i960 {
std::optional<uint16_t> reference_opcode_cycles(const Insn& instruction) {
    if (!instruction.executable()) return std::nullopt;
    switch (instruction.op->code) {
    case 0x008: return 1; // b
    case 0x009: return 0; // call (helper 9)
    case 0x00a: return 0; // ret (helper 7)
    case 0x00b: return 5; // bal
    case 0x010: return 1; // bno
    case 0x011: return 1; // bg
    case 0x012: return 1; // be
    case 0x013: return 1; // bge
    case 0x014: return 1; // bl
    case 0x015: return 1; // bne
    case 0x016: return 1; // ble
    case 0x017: return 1; // bo
    case 0x018: return 1; // faultno
    case 0x019: return 1; // faultg
    case 0x01a: return 1; // faulte
    case 0x01b: return 1; // faultge
    case 0x01c: return 1; // faultl
    case 0x01d: return 1; // faultne
    case 0x01e: return 1; // faultle
    case 0x01f: return 1; // faulto
    case 0x020: return 1; // testno
    case 0x021: return 1; // testg
    case 0x022: return 1; // teste
    case 0x023: return 1; // testge
    case 0x024: return 1; // testl
    case 0x025: return 1; // testne
    case 0x026: return 1; // testle
    case 0x027: return 1; // testo
    case 0x030: return 4; // bbc
    case 0x031: return 4; // cmp0bg
    case 0x032: return 4; // cmpobe
    case 0x033: return 4; // cmpobge
    case 0x034: return 4; // cmpobl
    case 0x035: return 4; // cmpobne
    case 0x036: return 4; // cmpoble
    case 0x037: return 4; // bbs
    case 0x039: return 4; // cmpibg
    case 0x03a: return 4; // cmpibe
    case 0x03b: return 4; // cmpibge
    case 0x03c: return 4; // cmpibl
    case 0x03d: return 4; // cmpibne
    case 0x03e: return 4; // cmpible
    case 0x080: return 4; // ldob
    case 0x082: return 2; // stob
    case 0x084: return 3; // bx
    case 0x085: return 5; // balx
    case 0x086: return 0; // callx (helper 9)
    case 0x088: return 4; // ldos
    case 0x08a: return 2; // stos
    case 0x08c: return 1; // lda
    case 0x090: return 4; // ld
    case 0x092: return 2; // st
    case 0x098: return 5; // ldl
    case 0x09a: return 3; // stl
    case 0x0a0: return 6; // ldt
    case 0x0a2: return 4; // stt
    case 0x0b0: return 7; // ldq
    case 0x0b2: return 5; // stq
    case 0x0c0: return 4; // ldib
    case 0x0c2: return 2; // stib
    case 0x0c8: return 4; // ldis
    case 0x0ca: return 2; // stis
    case 0x580: return 2; // notbit
    case 0x581: return 1; // and
    case 0x582: return 1; // andnot
    case 0x583: return 2; // setbit
    case 0x584: return 1; // notand
    case 0x586: return 1; // xor
    case 0x587: return 1; // or
    case 0x588: return 1; // nor
    case 0x589: return 1; // xnor
    case 0x58a: return 1; // not
    case 0x58b: return 1; // ornot
    case 0x58c: return 2; // clrbit
    case 0x58d: return 1; // notor
    case 0x58e: return 2; // nand
    case 0x58f: return 2; // alterbit
    case 0x590: return 1; // addo
    case 0x591: return 1; // addi
    case 0x592: return 1; // subo
    case 0x593: return 1; // subi
    case 0x598: return 1; // shro
    case 0x59a: return 1; // shrdi
    case 0x59b: return 1; // shri
    case 0x59c: return 1; // shlo
    case 0x59d: return 1; // rotate
    case 0x59e: return 1; // shli
    case 0x5a0: return 1; // cmpo
    case 0x5a1: return 1; // cmpi
    case 0x5a2: return 1; // concmpo
    case 0x5a3: return 1; // concmpi
    case 0x5a4: return 2; // cmpinco
    case 0x5a5: return 2; // cmpinci
    case 0x5a6: return 2; // cmpdeco
    case 0x5a7: return 2; // cmpdeci
    case 0x5ac: return 2; // scanbyte
    case 0x5ae: return 2; // chkbit
    case 0x5b0: return 2; // addc
    case 0x5b2: return 2; // subc
    case 0x5cc: return 2; // mov
    case 0x5dc: return 2; // movl
    case 0x5ec: return 3; // movt
    case 0x5fc: return 4; // movq
    case 0x600: return 6; // synmov
    case 0x602: return 12; // synmovq
    case 0x640: return 10; // spanbit
    case 0x641: return 10; // scanbit
    case 0x644: return 7; // dmovt
    case 0x645: return 10; // modac
    case 0x655: return 10; // modpc
    case 0x660: return 0; // calls (helper 9)
    case 0x66d: return 0; // flushreg (reference has no cycle deduction)
    case 0x670: return 37; // emul
    case 0x671: return 37; // ediv
    case 0x674: return 30; // cvtir
    case 0x675: return 30; // cvtilr
    case 0x676: return 30; // scalerl
    case 0x677: return 30; // scaler
    case 0x680: return 267; // atanr
    case 0x681: return 400; // logepr
    case 0x682: return 438; // logr
    case 0x683: return 67; // remr
    case 0x685: return 10; // cmpr
    case 0x688: return 104; // sqrtr
    case 0x689: return 334; // expr
    case 0x68a: return 37; // logbnr
    case 0x68b: return 69; // roundr
    case 0x68c: return 406; // sinr
    case 0x68d: return 406; // cosr
    case 0x68e: return 293; // tanr
    case 0x690: return 350; // atanrl
    case 0x692: return 438; // logrl
    case 0x695: return 12; // cmprl
    case 0x698: return 104; // sqrtrl
    case 0x699: return 334; // exprl
    case 0x69a: return 37; // logbnrl
    case 0x69b: return 70; // roundrl
    case 0x69c: return 441; // sinrl
    case 0x69d: return 441; // cosrl
    case 0x69e: return 323; // tanrl
    case 0x6c0: return 33; // cvtri
    case 0x6c1: return 35; // cvtril
    case 0x6c2: return 43; // cvtzri
    case 0x6c3: return 44; // cvtzril
    case 0x6c9: return 5; // movr
    case 0x6d9: return 6; // movrl
    case 0x6e1: return 8; // movre (undocumented encoding, used by Dead or Alive to save/restore fp0-fp3)
    case 0x6e2: return 8; // cpysre
    case 0x6e9: return 8; // movre
    case 0x701: return 18; // mulo
    case 0x708: return 37; // remo
    case 0x70b: return 37; // divo
    case 0x741: return 18; // muli
    case 0x748: return 37; // remi
    case 0x749: return 37; // modi
    case 0x74b: return 37; // divi
    case 0x78b: return 35; // divr
    case 0x78c: return 18; // mulr
    case 0x78d: return 10; // subr
    case 0x78f: return 10; // addr
    case 0x79b: return 77; // divrl
    case 0x79c: return 36; // mulrl
    case 0x79d: return 13; // subrl
    case 0x79f: return 13; // addrl
    default: return std::nullopt;
    }
}
} // namespace i960
