// license:BSD-3-Clause
// copyright-holders:Farfetch'd, R. Belmont
//
// TEST ORACLE ONLY. Never linked into the game.
//
// MAME's i960 instruction interpreter, transplanted from
// src/devices/cpu/i960/i960.cpp at dddd73680656e355bb2b5beecab1167c9f07bf81
// (BSD-3-Clause; notice above kept as the licence requires), over the
// runtime's Cpu context. m2replay runs the game through it to check the
// runtime and the replay against MAME independently of the recompiler.
// Changes: device framework and burst-stall machinery removed. Original
// opcode cycle deductions feed both m_icount (independent table metric) and
// Cpu::reference_cycles in their original positions. See THIRD_PARTY.md.

#include "refcore/i960_ref.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace rt {

// i960 Extended-real register image: words 0/1 hold the 64-bit fraction with
// an explicit integer bit in bit 63, word 2 holds the sign in bit 15 and the
// 15-bit biased exponent (bias 16383) in bits 14:0, with the upper 16 bits zero.
static void double_to_extended(double val, uint32_t *words)
{
	const uint64_t bits = d2u(val);
	const uint32_t sign = uint32_t(bits >> 63) << 15;
	uint64_t frac;
	uint32_t e;

	if(std::isnan(val) || std::isinf(val))
	{
		frac = (1ULL << 63) | ((bits & 0x000fffffffffffffULL) << 11);
		e = 0x7fff;
	}
	else if(val == 0.0)
	{
		frac = 0;
		e = 0;
	}
	else
	{
		int exp2;
		const double m = frexp(fabs(val), &exp2);   // 0.5 <= m < 1, exact for denormals too
		frac = uint64_t(ldexp(m, 64));              // integer bit lands in bit 63
		e = exp2 - 1 + 16383;
	}

	words[0] = uint32_t(frac);
	words[1] = uint32_t(frac >> 32);
	words[2] = sign | e;
}

static double extended_to_double(const uint32_t *words)
{
	const uint64_t frac = words[0] | (uint64_t(words[1]) << 32);
	const uint32_t e = words[2] & 0x7fff;
	double val;

	if(e == 0x7fff)
	{
		if(!(frac << 1))    // integer bit only: infinity
		{
			val = std::numeric_limits<double>::infinity();
		}
		else                // NaN: keep whatever payload fits, always quiet
		{
			val = u2d(0x7ff8000000000000ULL | ((frac & 0x7fffffffffffffffULL) >> 11));
		}
	}
	else
	{
		// value = fraction * 2^(exponent - bias - 63); e == 0 is a denormal (exponent 1)
		val = ldexp(double(frac), int(e ? e : 1) - 16383 - 63);
	}

	return BIT(words[2], 15) ? -val : val;
}


uint32_t I960Ref::get_ea(uint32_t opcode)
{
	int abase = (opcode >> 14) & 0x1f;
	if(!(opcode & 0x00001000)) { // MEMA
		uint32_t offset = opcode & 0x1fff;
		if(!(opcode & 0x2000))
			return offset;
		else
			return m_r[abase]+offset;
	} else {                     // MEMB
		int index = opcode & 0x1f;
		int scale = (opcode >> 7) & 0x7;
		int mode  = (opcode >> 10) & 0xf;
		uint32_t ret;

		switch(mode) {
		case 0x4:
			return m_r[abase];

		case 0x5:   // address of this instruction + the offset dword + 8
			// which in reality is "address of next instruction + the offset dword"
			ret = bus->fetch(m_IP);
			m_IP += 4;
			ret += m_IP;
			return ret;

		case 0x7:
			return m_r[abase] + (m_r[index] << scale);

		case 0xc:
			ret = bus->fetch(m_IP);
			m_IP += 4;
			return ret;

		case 0xd:
			ret = bus->fetch(m_IP) + m_r[abase];
			m_IP += 4;
			return ret;

		case 0xe:
			ret = bus->fetch(m_IP) + (m_r[index] << scale);
			m_IP += 4;
			return ret;

		case 0xf:
			ret = bus->fetch(m_IP) + m_r[abase] + (m_r[index] << scale);
			m_IP += 4;
			return ret;

		default:
			fatalerror("I960: %x: unhandled MEMB mode %x\n", m_PIP, mode);
		}
	}
}

uint32_t I960Ref::get_1_ri(uint32_t opcode)
{
	if(!(opcode & 0x00000800))
		return m_r[opcode & 0x1f];
	else
		return opcode & 0x1f;
}

uint32_t I960Ref::get_2_ri(uint32_t opcode)
{
	if(!(opcode & 0x00001000))
		return m_r[(opcode>>14) & 0x1f];
	else
		return (opcode>>14) & 0x1f;
}

uint64_t I960Ref::get_2_ri64(uint32_t opcode)
{
	if(!(opcode & 0x00001000))
		return m_r[(opcode>>14) & 0x1f] | ((uint64_t)m_r[((opcode>>14) & 0x1f)+1]<<32);
	else
		return (opcode>>14) & 0x1f;
}

void I960Ref::set_ri(uint32_t opcode, uint32_t val)
{
	if(!(opcode & 0x00002000))
		m_r[(opcode>>19) & 0x1f] = val;
	else {
		fatalerror("I960: %x: set_ri on literal?\n", m_PIP);
	}
}

void I960Ref::set_ri2(uint32_t opcode, uint32_t val, uint32_t val2)
{
	if(!(opcode & 0x00002000))
	{
		m_r[(opcode>>19) & 0x1f] = val;
		m_r[((opcode>>19) & 0x1f)+1] = val2;
	}
	else {
		fatalerror("I960: %x: set_ri2 on literal?\n", m_PIP);
	}
}

void I960Ref::set_ri64(uint32_t opcode, uint64_t val)
{
	if(!(opcode & 0x00002000)) {
		m_r[(opcode>>19) & 0x1f] = val;
		m_r[((opcode>>19) & 0x1f)+1] = val >> 32;
	} else
		fatalerror("I960: %x: set_ri64 on literal?\n", m_PIP);
}

double I960Ref::get_1_rif(uint32_t opcode)
{
	if(!(opcode & 0x00000800))
		return u2f(m_r[opcode & 0x1f]);
	else {
		int idx = opcode & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

double I960Ref::get_2_rif(uint32_t opcode)
{
	if(!(opcode & 0x00001000))
		return u2f(m_r[(opcode>>14) & 0x1f]);
	else {
		int idx = (opcode>>14) & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

void I960Ref::set_rif(uint32_t opcode, double val)
{
	if(!(opcode & 0x00002000))
		m_r[(opcode>>19) & 0x1f] = f2u(val);
	else if(!(opcode & 0x00e00000))
		m_fp[(opcode>>19) & 3] = val;
	else
		fatalerror("I960: %x: set_rif on literal?\n", m_PIP);
}

double I960Ref::get_1_rifl(uint32_t opcode)
{
	if(!(opcode & 0x00000800)) {
		uint64_t v = m_r[opcode & 0x1e];
		v |= ((uint64_t)(m_r[(opcode & 0x1e)+1]))<<32;
		return u2d(v);
	} else {
		int idx = opcode & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

double I960Ref::get_2_rifl(uint32_t opcode)
{
	if(!(opcode & 0x00001000)) {
		uint64_t v = m_r[(opcode >> 14) & 0x1e];
		v |= ((uint64_t)(m_r[((opcode>>14) & 0x1e)+1]))<<32;
		return u2d(v);
	} else {
		int idx = (opcode>>14) & 0x1f;
		if(idx < 4)
			return m_fp[idx];
		if(idx == 0x16)
			return 1.0;
		return 0.0;
	}
}

void I960Ref::set_rifl(uint32_t opcode, double val)
{
	if(!(opcode & 0x00002000)) {
		uint64_t v = d2u(val);
		m_r[(opcode>>19) & 0x1e] = v;
		m_r[((opcode>>19) & 0x1e)+1] = v>>32;
	} else if(!(opcode & 0x00e00000))
		m_fp[(opcode>>19) & 3] = val;
	else
		fatalerror("I960: %x: set_rifl on literal?\n", m_PIP);
}

uint32_t I960Ref::get_1_ci(uint32_t opcode)
{
	if(!(opcode & 0x00002000))
		return m_r[(opcode >> 19) & 0x1f];
	else
		return (opcode >> 19) & 0x1f;
}

uint32_t I960Ref::get_2_ci(uint32_t opcode)
{
	return m_r[(opcode >> 14) & 0x1f];
}

uint32_t I960Ref::get_disp(uint32_t opcode)
{
	return sext(opcode, 24) - 4;
}

uint32_t I960Ref::get_disp_s(uint32_t opcode)
{
	return sext(opcode, 13) - 4;
}

void I960Ref::cmp_s(int32_t v1, int32_t v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

void I960Ref::cmp_u(uint32_t v1, uint32_t v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

void I960Ref::concmp_s(int32_t v1, int32_t v2)
{
	m_AC &= ~7;
	if(v1 <= v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

void I960Ref::concmp_u(uint32_t v1, uint32_t v2)
{
	m_AC &= ~7;
	if(v1 <= v2)
		m_AC |= 2;
	else
		m_AC |= 1;
}

void I960Ref::cmp_d(double v1, double v2)
{
	m_AC &= ~7;
	if(v1<v2)
		m_AC |= 4;
	else if(v1 == v2)
		m_AC |= 2;
	else if(v1 > v2)
		m_AC |= 1;
}

void I960Ref::bxx(uint32_t opcode, int mask)
{
	if(m_AC & mask) {
		m_IP += get_disp(opcode);
		m_IP &= ~3;
	}
}

void I960Ref::fxx(uint32_t opcode, int mask)
{
	if(m_AC & mask) {
		fatalerror("Taking the fault on a FAULT insn not yet supported\n");
	}
}

void I960Ref::bxx_s(uint32_t opcode, int mask)
{
	if(m_AC & mask) {
		m_IP += get_disp_s(opcode);
		m_IP &= ~3;
	}
}

void I960Ref::test(uint32_t opcode, int mask)
{
	if(m_AC & mask)
		m_r[(opcode>>19) & 0x1f] = 1;
	else
		m_r[(opcode>>19) & 0x1f] = 0;
}

double I960Ref::round_to_int(double val)
{
	// apply rounding mode
	switch ((m_AC >> 30) & 3)
	{
	case 0: return round(val);
	case 1: return floor(val);
	case 2: return ceil(val);
	default: return trunc(val);
	}
}

void I960Ref::burst_stall_save(uint32_t t1, uint32_t t2, int index, int size, bool iswriteop)
{
	m_stall_state.t1 = t1;
	m_stall_state.t2 = t2;
	m_stall_state.index = index;
	m_stall_state.size = size;
	m_stall_state.iswriteop = iswriteop;
	m_stall_state.burst_mode = true;
}

void I960Ref::execute_op(uint32_t opcode)
{
	uint32_t t1, t2;
	double t1f, t2f;

	switch(opcode >> 24) {
		case 0x08: // b
			consume_instruction_cycles(1);
			m_IP += get_disp(opcode);
			break;

		case 0x09: // call
			do_call(m_IP+get_disp(opcode), 0, m_r[I960_SP]);
			break;

		case 0x0a: // ret
			do_ret();
			break;

		case 0x0b: // bal
			consume_instruction_cycles(5);
			m_r[0x1e] = m_IP;
			m_IP += get_disp(opcode);
			break;

		case 0x10: // bno
			consume_instruction_cycles(1);
			if(!(m_AC & 7)) {
				m_IP += get_disp(opcode);
			}
			break;

		case 0x11: // bg
			consume_instruction_cycles(1);
			bxx(opcode, 1);
			break;

		case 0x12: // be
			consume_instruction_cycles(1);
			bxx(opcode, 2);
			break;

		case 0x13: // bge
			consume_instruction_cycles(1);
			bxx(opcode, 3);
			break;

		case 0x14: // bl
			consume_instruction_cycles(1);
			bxx(opcode, 4);
			break;

		case 0x15: // bne
			consume_instruction_cycles(1);
			bxx(opcode, 5);
			break;

		case 0x16: // ble
			consume_instruction_cycles(1);
			bxx(opcode, 6);
			break;

		case 0x17: // bo
			consume_instruction_cycles(1);
			bxx(opcode, 7);
			break;

		case 0x18: // faultno
			consume_instruction_cycles(1);
			if(!(m_AC & 7)) {
				m_IP += get_disp(opcode);
			}
			break;

		case 0x19: // faultg
			consume_instruction_cycles(1);
			fxx(opcode, 1);
			break;

		case 0x1a: // faulte
			consume_instruction_cycles(1);
			fxx(opcode, 2);
			break;

		case 0x1b: // faultge
			consume_instruction_cycles(1);
			fxx(opcode, 3);
			break;

		case 0x1c: // faultl
			consume_instruction_cycles(1);
			fxx(opcode, 4);
			break;

		case 0x1d: // faultne
			consume_instruction_cycles(1);
			fxx(opcode, 5);
			break;

		case 0x1e: // faultle
			consume_instruction_cycles(1);
			fxx(opcode, 6);
			break;

		case 0x1f: // faulto
			consume_instruction_cycles(1);
			fxx(opcode, 7);
			break;

		case 0x20: // testno
			consume_instruction_cycles(1);
			if(!(m_AC & 7))
				m_r[(opcode>>19) & 0x1f] = 1;
			else
				m_r[(opcode>>19) & 0x1f] = 0;
			break;

		case 0x21: // testg
			consume_instruction_cycles(1);
			test(opcode, 1);
			break;

		case 0x22: // teste
			consume_instruction_cycles(1);
			test(opcode, 2);
			break;

		case 0x23: // testge
			consume_instruction_cycles(1);
			test(opcode, 3);
			break;

		case 0x24: // testl
			consume_instruction_cycles(1);
			test(opcode, 4);
			break;

		case 0x25: // testne
			consume_instruction_cycles(1);
			test(opcode, 5);
			break;

		case 0x26: // testle
			consume_instruction_cycles(1);
			test(opcode, 6);
			break;

		case 0x27: // testo
			consume_instruction_cycles(1);
			test(opcode, 7);
			break;

		case 0x30: // bbc
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode) & 0x1f;
			t2 = get_2_ci(opcode);
			if(!(t2 & (1<<t1))) {
				m_AC = (m_AC & ~7) | 2;
				m_IP += get_disp_s(opcode);
			} else
				m_AC &= ~7;
			break;

		case 0x31: // cmp0bg
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 1);
			break;

		case 0x32: // cmpobe
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 2);
			break;

		case 0x33: // cmpobge
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 3);
			break;

		case 0x34: // cmpobl
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 4);
			break;

		case 0x35: // cmpobne
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 5);
			break;

		case 0x36: // cmpoble
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_u(t1, t2);
			bxx_s(opcode, 6);
			break;

		case 0x37: // bbs
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode) & 0x1f;
			t2 = get_2_ci(opcode);
			if(t2 & (1<<t1)) {
				m_AC = (m_AC & ~7) | 2;
				m_IP += get_disp_s(opcode);
			} else
				m_AC &= ~7;
			break;

		case 0x39: // cmpibg
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 1);
			break;

		case 0x3a: // cmpibe
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 2);
			break;

		case 0x3b: // cmpibge
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 3);
			break;

		case 0x3c: // cmpibl
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 4);
			break;

		case 0x3d: // cmpibne
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 5);
			break;

		case 0x3e: // cmpible
			consume_instruction_cycles(4);
			t1 = get_1_ci(opcode);
			t2 = get_2_ci(opcode);
			cmp_s(t1, t2);
			bxx_s(opcode, 6);
			break;

		case 0x58:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // notbit
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 ^ (1<<(t1 & 31)));
				break;

			case 0x1: // and
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & t1);
				break;

			case 0x2: // andnot
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & ~t1);
				break;

			case 0x3: // setbit
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | (1<<(t1 & 31)));
				break;

			case 0x4: // notand
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, (~t2) & t1);
				break;

			case 0x6: // xor
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 ^ t1);
				break;

			case 0x7: // or
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | t1);
				break;

			case 0x8: // nor
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((~t2) & (~t1)));
				break;

			case 0x9: // xnor
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ~(t2 ^ t1));
				break;

			case 0xa: // not
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				set_ri(opcode, ~t1);
				break;

			case 0xb: // ornot
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 | ~t1);
				break;

			case 0xc: // clrbit
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2 & ~(1<<(t1 & 31)));
				break;

			case 0xd: // notor
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, (~t2) | t1);
				break;

			case 0xe: // nand
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ~t2 | ~t1);
				break;

			case 0xf: // alterbit
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(m_AC & 2)
					set_ri(opcode, t2 | (1<<(t1 & 31)));
				else
					set_ri(opcode, t2 & ~(1<<(t1 & 31)));
				break;

			default:
				fatalerror("I960: %x: Unhandled 58.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x59:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // addo
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2+t1);
				break;

			case 0x1: // addi
				// #### overflow
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2+t1);
				break;

			case 0x2: // subo
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2-t1);
				break;

			case 0x3: // subi
				// #### overflow
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2-t1);
				break;

			case 0x8: // shro
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t1 >= 32 ? 0 : t2>>t1);
				break;

			case 0xa: // shrdi
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 >= 32)
					set_ri(opcode, 0);
				else if(((int32_t)t2) < 0) {
					if(t2 & ((1<<t1)-1))
						set_ri(opcode, (((int32_t)t2)>>t1)+1);
					else
						set_ri(opcode, ((int32_t)t2)>>t1);
				} else
					set_ri(opcode, t2>>t1);
				break;

			case 0xb: // shri
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 >= 32)
					set_ri(opcode, (int32_t)t2 < 0 ? -1 : 0);
				else
					set_ri(opcode, ((int32_t)t2)>>t1);
				break;

			case 0xc: // shlo
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t1 >= 32 ? 0 : t2<<t1);
				break;

			case 0xd: // rotate
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode) & 0x1f;
				t2 = get_2_ri(opcode);
				set_ri(opcode, std::rotl(t2, t1));
				break;

			case 0xe: // shli
				// missing overflow
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				// TODO: on later models, sign is always preserved even upon overflow
				set_ri(opcode, t1 >= 32 ? 0 : t2<<t1);
				break;

			default:
				fatalerror("I960: %x: Unhandled 59.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5a:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // cmpo
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				break;

			case 0x1: // cmpi
				consume_instruction_cycles(1);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				break;

			case 0x2: // concmpo
				consume_instruction_cycles(1);
				if(!(m_AC & 0x4)) {
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					concmp_u(t1, t2);
				}
				break;

			case 0x3: // concmpi
				consume_instruction_cycles(1);
				if(!(m_AC & 0x4)) {
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					concmp_s(t1, t2);
				}
				break;

			case 0x4: // cmpinco
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				set_ri(opcode, t2+1);
				break;

			case 0x5: // cmpinci
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				set_ri(opcode, t2+1);
				break;

			case 0x6: // cmpdeco
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_u(t1, t2);
				set_ri(opcode, t2-1);
				break;

			case 0x7: // cmpdeci
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				cmp_s(t1, t2);
				set_ri(opcode, t2-1);
				break;

			case 0xc: // scanbyte
				consume_instruction_cycles(2);
				m_AC &= ~7;     // clear CC
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if ((t1 & 0xff000000) == (t2 & 0xff000000) ||
					(t1 & 0x00ff0000) == (t2 & 0x00ff0000) ||
					(t1 & 0x0000ff00) == (t2 & 0x0000ff00) ||
					(t1 & 0x000000ff) == (t2 & 0x000000ff))
				{
					m_AC |= 2;
				}
				break;

			case 0xe: // chkbit
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode) & 0x1f;
				t2 = get_2_ri(opcode);
				if(t2 & (1<<t1))
					m_AC = (m_AC & ~7) | 2;
				else
					m_AC &= ~7;
				break;

			default:
				fatalerror("I960: %x: Unhandled 5a.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5b:
			switch((opcode >> 7) & 0xf) {
			case 0x0:   // addc
				{
					uint64_t res;

					consume_instruction_cycles(2);
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					res = t2+(t1+((m_AC>>1)&1));
					set_ri(opcode, res&0xffffffff);

					m_AC &= ~0x3;   // clear C and V
					// set carry
					m_AC |= ((res) & (((uint64_t)1) << 32)) ? 0x2 : 0;
					// set overflow
					m_AC |= (((res) ^ (t1)) & ((res) ^ (t2)) & 0x80000000) ? 1: 0;
				}
				break;

			case 0x2:   // subc
				{
					uint64_t res;

					consume_instruction_cycles(2);
					t1 = get_1_ri(opcode);
					t2 = get_2_ri(opcode);
					// dst = src2 - src1 - 1 + C, or src2 + ~src1 + C
					res = (uint64_t)t2 + (uint64_t)(uint32_t)~t1 + ((m_AC>>1)&1);
					set_ri(opcode, res&0xffffffff);

					m_AC &= ~0x7;   // cc = 0CV
					// set carry
					m_AC |= ((res) & (((uint64_t)1) << 32)) ? 0x2 : 0;
					// set overflow
					m_AC |= (((t2) ^ (t1)) & ((t2) ^ (res)) & 0x80000000) ? 1 : 0;
				}
				break;

			default:
				fatalerror("I960: %x: Unhandled 5b.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5c:
			switch((opcode >> 7) & 0xf) {
			case 0xc: // mov
				consume_instruction_cycles(2);
				t1 = get_1_ri(opcode);
				set_ri(opcode, t1);
				break;

			default:
				fatalerror("I960: %x: Unhandled 5c.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5d:
			switch((opcode >> 7) & 0xf) {
			case 0xc: // movl
				consume_instruction_cycles(2);
				t2 = (opcode>>19) & 0x1e;
				if(opcode & 0x00000800) { // litteral
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 2*sizeof(uint32_t));
				break;

			default:
				fatalerror("I960: %x: Unhandled 5d.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5e:
			switch((opcode >> 7) & 0xf) {
			case 0xc: // movt
				consume_instruction_cycles(3);
				t2 = (opcode>>19) & 0x1c;
				if(opcode & 0x00000800) { // litteral
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = m_r[t2+2]= t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 3*sizeof(uint32_t));
				break;

			default:
				fatalerror("I960: %x: Unhandled 5e.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x5f:
			switch((opcode >> 7) & 0xf) {
			case 0xc: // movq
				consume_instruction_cycles(4);
				t2 = (opcode>>19) & 0x1c;
				if(opcode & 0x00000800) { // litteral
					t1 = opcode & 0x1f;
					m_r[t2] = m_r[t2+1] = m_r[t2+2] = m_r[t2+3] = t1;
				} else
					memcpy(m_r+t2, m_r+(opcode & 0x1f), 4*sizeof(uint32_t));
				break;

			default:
				fatalerror("I960: %x: Unhandled 5f.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x60:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // synmov
				consume_instruction_cycles(6);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				// interrupt control register
				if(t1 == 0xff000004)
					m_ICR = bus->read_dword(t2);
				else
					bus->write_dword(t1,    bus->read_dword(t2));
				m_AC = (m_AC & ~7) | 2;
				break;

			case 0x2: // synmovq
				consume_instruction_cycles(12);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if(t1 == 0xff000010)
					send_iac(t2);
				else {
					bus->write_dword(t1,    bus->read_dword(t2));
					bus->write_dword(t1+4,  bus->read_dword(t2+4));
					bus->write_dword(t1+8,  bus->read_dword(t2+8));
					bus->write_dword(t1+12, bus->read_dword(t2+12));
				}
				m_AC = (m_AC & ~7) | 2;
				break;

			default:
				fatalerror("I960: %x: Unhandled 60.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x64:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // spanbit
				{
					uint32_t res = 0xffffffff;
					int i;

					consume_instruction_cycles(10);

					t1 = get_1_ri(opcode);
					m_AC &= ~7;

					for (i = 31; i >= 0; i--)
					{
						if (!(t1 & (1<<i)))
						{
							m_AC |= 2;
							res = i;
							break;
						}
					}

					set_ri(opcode, res);
				}
				break;

			case 0x1: // scanbit
				{
					uint32_t res = 0xffffffff;
					int i;

					consume_instruction_cycles(10);

					t1 = get_1_ri(opcode);
					m_AC &= ~7;

					for (i = 31; i >= 0; i--)
					{
						if (t1 & (1<<i))
						{
							m_AC |= 2;
							res = i;
							break;
						}
					}

					set_ri(opcode, res);
				}
				break;

			case 0x4: // dmovt
				/*
				    The dmovt instruction moves a 32-bit word from one register to another
				    and tests the least-significant byte of the operand to determine if it is a
				    valid ASCII-coded decimal digit (001100002 through 001110012,
				    corresponding to the decimal digits 0 through 9). For valid digits, the
				    condition code (CC) is set to 000; otherwise the condition code is set to
				    010.
				*/
				consume_instruction_cycles(7);
				t1 = get_1_ri(opcode);
				set_ri(opcode, t1);
				m_AC &= 0xfff8;
				if ((t1 & 0xff) < 0x30 || (t1 & 0xff) > 0x39)
					m_AC |= 2;
				break;

			case 0x5: // modac
				consume_instruction_cycles(10);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, m_AC);
				m_AC = (m_AC & ~t1) | (t2 & t1);
				break;

			default:
				fatalerror("I960: %x: Unhandled 64.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x65:
			switch((opcode >> 7) & 0xf) {
			case 0x5: // modpc
				consume_instruction_cycles(10);
				t1 = m_PC;
				t2 = get_2_ri(opcode);
				m_PC = (m_PC & ~t2) | (m_r[(opcode>>19) & 0x1f] & t2);
				set_ri(opcode, t1);
				if ((t1 >> 16 & 0x1f) > (m_PC >> 16 & 0x1f))
					check_pending_irqs();
				break;

			default:
				fatalerror("I960: %x: Unhandled 65.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x66:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // calls
				t1 = get_1_ri(opcode);
				t2 = bus->read_dword(m_SAT + 152);    // get pointer to system procedure table
				t2 = bus->read_dword(t2 + 48 + (t1 * 4));
				if ((t2 & 3) != 0)
				{
					fatalerror("I960: system calls that jump into supervisor mode aren't yet supported\n");
				}
				do_call(t2, 0, m_r[I960_SP]);
				break;

			case 0xd: // flushreg
				if (m_rcache_pos > 4)
				{
					m_rcache_pos = 4;
				}
				for(t1=0; t1 < m_rcache_pos; t1++)
				{
					int i;

					for (i = 0; i < 0x10; i++)
					{
						bus->write_dword(m_rcache_frame_addr[t1] + (i * sizeof(uint32_t)), m_rcache[t1][i]);
					}
				}
				m_rcache_pos = 0;
				break;

			default:
				fatalerror("I960: %x: Unhandled 66.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x67:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // emul
				consume_instruction_cycles(37);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);

				set_ri64(opcode, mulu_32x32(t1, t2));
				break;

			case 0x1: // ediv
				consume_instruction_cycles(37);
				{
					uint64_t src1, src2;

					src1 = get_1_ri(opcode);
					src2 = get_2_ri64(opcode);

					set_ri2(opcode, src2 % src1, src2 / src1);
				}
				break;

			case 0x4: // cvtir
				consume_instruction_cycles(30);
				t1 = get_1_ri(opcode);
				set_rif(opcode, (double)(int32_t)t1);
				break;

			case 0x5: // cvtilr
				consume_instruction_cycles(30);
				t1 = get_1_ri(opcode);
				set_rifl(opcode, (double)(int32_t)t1);
				break;

			case 0x6: // scalerl
				consume_instruction_cycles(30);
				t1 = get_1_ri(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f * pow(2.0, (double)(int32_t)t1));
				break;

			case 0x7: // scaler
				consume_instruction_cycles(30);
				t1 = get_1_ri(opcode);
				t2f = get_2_rif(opcode);
			set_rif(opcode, t2f * pow(2.0, (double)(int32_t)t1));
				break;

			default:
				fatalerror("I960: %x: Unhandled 67.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x68:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // atanr
				consume_instruction_cycles(267);
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, atan2(t2f, t1f));
				break;

			case 0x1: // logepr
				consume_instruction_cycles(400);
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*log2(t1f+1.0));
				break;

			case 0x2: // logr
				consume_instruction_cycles(438);
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*log2(t1f));
				break;

			case 0x3: // remr
				consume_instruction_cycles(67); // (67 to 75878 depending on opcodes!!!)
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, fmod(t2f, t1f));
				break;

			case 0x5: // cmpr
				consume_instruction_cycles(10);
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				cmp_d(t1f, t2f);
				break;

			case 0x8: // sqrtr
				consume_instruction_cycles(104);
				t1f = get_1_rif(opcode);
				set_rif(opcode, sqrt(t1f));
				break;

			case 0x9: // expr
				consume_instruction_cycles(334); // checkme
				t1f = get_1_rif(opcode);
				set_rif(opcode, pow(2.0, t1f) - 1.0);
				break;

			case 0xa: // logbnr
				consume_instruction_cycles(37);
				t1f = get_1_rif(opcode);
				set_rif(opcode, logb(t1f));
				break;

			case 0xb: // roundr
				consume_instruction_cycles(69);
				t1f = get_1_rif(opcode);
				set_rif(opcode, round_to_int(t1f));
				break;

			case 0xc: // sinr
				consume_instruction_cycles(406);
				t1f = get_1_rif(opcode);
				set_rif(opcode, sin(t1f));
				break;

			case 0xd: // cosr
				consume_instruction_cycles(406);
				t1f = get_1_rif(opcode);
				set_rif(opcode, cos(t1f));
				break;

			case 0xe: // tanr
				consume_instruction_cycles(293);
				t1f = get_1_rif(opcode);
				set_rif(opcode, tan(t1f));
				break;

			default:
				fatalerror("I960: %x: Unhandled 68.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x69:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // atanrl
				consume_instruction_cycles(350);
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, atan2(t2f, t1f));
				break;

			case 0x2: // logrl
				consume_instruction_cycles(438);
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f*log2(t1f));
				break;

			case 0x5: // cmprl
				consume_instruction_cycles(12);
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				cmp_d(t1f, t2f);
				break;

			case 0x8: // sqrtrl
				consume_instruction_cycles(104);
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, sqrt(t1f));
				break;

			case 0x9: // exprl
				consume_instruction_cycles(334);
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, pow(2.0, t1f)-1.0);
				break;

			case 0xa: // logbnrl
				consume_instruction_cycles(37);
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, logb(t1f));
				break;

			case 0xb: // roundrl
				consume_instruction_cycles(70);
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, round_to_int(t1f));
				break;

			case 0xc: // sinrl
				consume_instruction_cycles(441);
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, sin(t1f));
				break;

			case 0xd: // cosrl
				consume_instruction_cycles(441);
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, cos(t1f));
				break;

			case 0xe: // tanrl
				consume_instruction_cycles(323);
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, tan(t1f));
				break;

			default:
				fatalerror("I960: %x: Unhandled 69.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6c:
			switch((opcode >> 7) & 0xf) {
			case 0x0: // cvtri
				consume_instruction_cycles(33);
				t1f = get_1_rif(opcode);
				set_ri(opcode, (int32_t)round_to_int(t1f));
				break;

			case 0x1: // cvtril
				consume_instruction_cycles(35);
				t1f = get_1_rif(opcode);
				set_ri64(opcode, (int64_t)round_to_int(t1f));
				break;

			case 0x2: // cvtzri
				consume_instruction_cycles(43);
				t1f = get_1_rif(opcode);
				set_ri(opcode, (int32_t)t1f);
				break;

			case 0x3: // cvtzril
				consume_instruction_cycles(44);
				t1f = get_1_rif(opcode);
				set_ri64(opcode, (int64_t)t1f);
				break;

			case 0x9: // movr
				consume_instruction_cycles(5);
				t1f = get_1_rif(opcode);
				set_rif(opcode, t1f);
				break;

			default:
				fatalerror("I960: %x: Unhandled 6c.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6d:
			switch((opcode >> 7) & 0xf) {
			case 0x9: // movrl
				consume_instruction_cycles(6);
				t1f = get_1_rifl(opcode);
				set_rifl(opcode, t1f);
				break;

			default:
				fatalerror("I960: %x: Unhandled 6d.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x6e:
			switch((opcode >> 7) & 0xf) {
			case 0x1: // movre (undocumented encoding, used by Dead or Alive to save/restore fp0-fp3)
			case 0x9: // movre
				{
					uint32_t v[3];

					consume_instruction_cycles(8);

					// source: three g/l registers (multiple of 4), a floating-point register or a literal
					if(!(opcode & 0x00000800))
					{
						const int src = opcode & 0x1c;
						v[0] = m_r[src];
						v[1] = m_r[src+1];
						v[2] = m_r[src+2] & 0xffff;    // upper 16 bits of the third word are truncated
					}
					else
					{
						const int idx = opcode & 0x1f;
						if(idx < 4)
							double_to_extended(m_fp[idx], v);
						else
							double_to_extended((idx == 0x16) ? 1.0 : 0.0, v);
					}

					// destination: three g/l registers (multiple of 4) or a floating-point register
					if(!(opcode & 0x00002000))
					{
						const int dst = (opcode>>19) & 0x1c;
						m_r[dst] = v[0];
						m_r[dst+1] = v[1];
						m_r[dst+2] = v[2];
					}
					else if(!(opcode & 0x00e00000))
						m_fp[(opcode>>19) & 3] = extended_to_double(v);
					else
						fatalerror("i960: %x: movre to literal?\n", m_PIP);
				}
				break;
			case 0x2: // cpysre
				consume_instruction_cycles(8);
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);

				if (t2f >= 0.0)
					set_rifl(opcode, fabs(t1f));
				else
					set_rifl(opcode, -fabs(t1f));
				break;
			default:
				fatalerror("I960: %x: Unhandled 6e.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x70:
			switch((opcode >> 7) & 0xf) {
			case 0x1: // mulo
				consume_instruction_cycles(18);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2*t1);
				break;

			case 0x8: // remo
				consume_instruction_cycles(37);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, t2%t1);
				break;

			case 0xb: // divo
				consume_instruction_cycles(37);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				if (t1 == 0)    // HACK!
					set_ri(opcode, 0);
				else
					set_ri(opcode, t2/t1);
				break;

			default:
				fatalerror("I960: %x: Unhandled 70.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x74:
			switch((opcode >> 7) & 0xf) {
			case 0x1: // muli
				consume_instruction_cycles(18);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((int32_t)t2)*((int32_t)t1));
				break;

			case 0x8: // remi
				consume_instruction_cycles(37);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((int32_t)t2)%((int32_t)t1));
				break;

			case 0x9:{// modi
				int32_t src1, src2, dst;
				consume_instruction_cycles(37);
				src1 = (int32_t)get_1_ri(opcode);
				src2 = (int32_t)get_2_ri(opcode);
				dst = src2 - ((src2/src1)*src1);
				if(((src1 ^ src2) < 0) && (dst != 0))   // operands of opposite sign (the product would overflow)
					dst += src1;
				set_ri(opcode, dst);
				break;
			}

			case 0xb: // divi
				consume_instruction_cycles(37);
				t1 = get_1_ri(opcode);
				t2 = get_2_ri(opcode);
				set_ri(opcode, ((int32_t)t2)/((int32_t)t1));
				break;

			default:
				fatalerror("I960: %x: Unhandled 74.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x78:
			switch((opcode >> 7) & 0xf) {
			case 0xb: // divr
				consume_instruction_cycles(35);
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f/t1f);
				break;

			case 0xc: // mulr
				consume_instruction_cycles(18);
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f*t1f);
				break;

			case 0xd: // subr
				consume_instruction_cycles(10);
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f-t1f);
				break;

			case 0xf: // addr
				consume_instruction_cycles(10);
				t1f = get_1_rif(opcode);
				t2f = get_2_rif(opcode);
				set_rif(opcode, t2f+t1f);
				break;

			default:
				fatalerror("I960: %x: Unhandled 78.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x79:
			switch((opcode >> 7) & 0xf) {
			case 0xb: // divrl
				consume_instruction_cycles(77);
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f/t1f);
				break;

			case 0xc: // mulrl
				consume_instruction_cycles(36);
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f*t1f);
				break;

			case 0xd: // subrl
				consume_instruction_cycles(13);
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f-t1f);
				break;

			case 0xf: // addrl
				consume_instruction_cycles(13);
				t1f = get_1_rifl(opcode);
				t2f = get_2_rifl(opcode);
				set_rifl(opcode, t2f+t1f);
				break;

			default:
				fatalerror("I960: %x: Unhandled 79.%x\n", m_PIP, (opcode >> 7) & 0xf);
			}
			break;

		case 0x80: { // ldob
			consume_instruction_cycles(4);
			u8 v = bus->read_byte(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x82: // stob
			consume_instruction_cycles(2);
			bus->write_byte(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x84: // bx
			consume_instruction_cycles(3);
			m_IP = get_ea(opcode);
			break;

		case 0x85: // balx
			consume_instruction_cycles(5);
			t1 = get_ea(opcode);
			m_r[(opcode>>19)&0x1f] = m_IP;
			m_IP = t1;
			break;

		case 0x86: // callx
			t1 = get_ea(opcode);
			do_call(t1, 0, m_r[I960_SP]);
			break;

		case 0x88: { // ldos
			consume_instruction_cycles(4);
			u16 v = i960_read_word_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x8a: // stos
			consume_instruction_cycles(2);
			i960_write_word_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x8c: // lda
			consume_instruction_cycles(1);
			m_r[(opcode>>19)&0x1f] = get_ea(opcode);
			break;

		case 0x90: { // ld
			consume_instruction_cycles(4);
			u32 v = i960_read_dword_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0x92: // st
			consume_instruction_cycles(2);
			i960_write_dword_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0x98:{// ldl
			int i;
			consume_instruction_cycles(5);
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1e;
			for(i=0; i<2; i++) {
				auto pack = i960_read_dword_unaligned_flags(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,2,false);
					return;
				}
				m_r[t2+i] = pack.first;
				if(pack.second & BURST)
					t1 += 4;
			}
			break;
		}

		case 0x9a:{// stl
			int i;
			consume_instruction_cycles(3);
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1e;
			for(i=0; i<2; i++) {
				auto flags = i960_write_dword_unaligned_flags(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,2,true);
					return;
				}
				if(flags & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xa0:{// ldt
			int i;
			consume_instruction_cycles(6);
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<3; i++) {
				auto pack = i960_read_dword_unaligned_flags(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,3,false);
					return;
				}
				m_r[t2+i] = pack.first;
				if(pack.second & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xa2:{// stt
			int i;
			consume_instruction_cycles(4);
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<3; i++) {
				auto flags = i960_write_dword_unaligned_flags(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,3,true);
					return;
				}
				if(flags & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xb0:{// ldq
			int i;
			consume_instruction_cycles(7);
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<4; i++) {
				auto pack = i960_read_dword_unaligned_flags(t1);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,4,false);
					return;
				}
				m_r[t2+i] = pack.first;
				if(pack.second & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xb2:{// stq
			int i;
			consume_instruction_cycles(5);
			t1 = get_ea(opcode);
			t2 = (opcode>>19)&0x1c;
			for(i=0; i<4; i++) {
				auto flags = i960_write_dword_unaligned_flags(t1, m_r[t2+i]);
				if(m_stalled)
				{
					burst_stall_save(t1,t2,i,4,true);
					return;
				}
				if(flags & BURST)
					t1 += 4;
			}
			break;
		}

		case 0xc0: { // ldib
			consume_instruction_cycles(4);
			s8 v = bus->read_byte(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0xc2: // stib
			consume_instruction_cycles(2);
			bus->write_byte(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		case 0xc8: { // ldis
			consume_instruction_cycles(4);
			s16 v = i960_read_word_unaligned(get_ea(opcode));
			if(!m_stalled)
				m_r[(opcode>>19)&0x1f] = v;
			break;
		}

		case 0xca: // stis
			consume_instruction_cycles(2);
			i960_write_word_unaligned(get_ea(opcode), m_r[(opcode>>19)&0x1f]);
			break;

		default:
			fatalerror("I960: %x: Unhandled %02x\n", m_PIP, opcode >> 24);
	}

}

// One pass of MAME's execute_run loop body (burst-stall path removed).
void I960Ref::execute_one()
{
	m_PIP = m_IP;
	const uint32_t opcode = bus->fetch(m_IP);
	m_IP += 4;
	m_stalled = false;
	execute_op(opcode);
}

} // namespace rt
