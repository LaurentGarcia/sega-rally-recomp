// license:BSD-3-Clause
// copyright-holders:ElSemi, R. Belmont
// scspdsp.h SHA256 db522ecaa04665063e46f7f6c699eb7781c672fd7624ac58a60b0b180652d3cc
// emumem.h SHA256 4285ca50a0c14b6038c7443bab45340834f0e4b741f0549a127c4ba7cc1c3e2e
#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
namespace snd::scsp_detail {
using u8=uint8_t; using s8=int8_t; using u16=uint16_t; using s16=int16_t;
using u32=uint32_t; using s32=int32_t; using u64=uint64_t; using s64=int64_t;
template<class T> constexpr u32 BIT(T v,unsigned bit) { return (u32(v)>>bit)&1; }
template<class T> constexpr s32 sext(T value,unsigned bits) {
 const u32 low=u32(value)&((u32(1)<<bits)-1), sign=u32(1)<<(bits-1);
 return s32((low^sign)-sign);
}
// SCSP's 20-bit, 16-bit big-endian space has only 0..0x7ffff RAM mapped.
// The reference's aligned word cache ignores address bit0 (emumem.h:1720);
// the Model2A map leaves the upper 512 KiB unmapped, reading zero.
struct ScspRam {
 std::span<u8> bytes;
 u8 read_byte(u32 a) const { a&=0xfffff; return a<bytes.size()?bytes[a]:0; }
 u16 read_word(u32 a) const { a&=0xffffe; return u16(u16(read_byte(a))<<8|read_byte(a+1)); }
 void write_word(u32 a,u16 value) { a&=0xffffe; if(a<bytes.size()) {bytes[a]=u8(value>>8);bytes[a+1]=u8(value);} }
};
// Native adaptation of MAME dddd73680656e355bb2b5beecab1167c9f07bf81.
// Upstream SCSP/DSP semantics and documented imperfections are retained.
//the DSP Context
struct Model2ScspDsp
{
//Config
	ScspRam *space;
	u32 RBP; //Ring buf pointer
	u32 RBL; //Delay ram (Ring buffer) size in words

//context

	s16 COEF[64];     //16 bit signed
	u16 MADRS[32];   //offsets (in words), 16 bit
	u16 MPRO[128*4]; //128 steps 64 bit
	s32 TEMP[128];    //TEMP regs,24 bit signed
	s32 MEMS[32]; //MEMS regs,24 bit signed
	u32 DEC;

//input
	s32 MIXS[16]; //MIXS, 24 bit signed
	s16 EXTS[2];  //External inputs (CDDA)    16 bit signed

//output
	s16 EFREG[16];    //EFREG, 16 bit signed

	bool Stopped;
	int LastStep;

	void Init();
	void SetSample(s32 sample, s32 SEL, s32 MXL);
	void Step();
	void Start();
};


} // namespace snd::scsp_detail
