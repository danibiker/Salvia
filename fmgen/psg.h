// ---------------------------------------------------------------------------
//	PSG-like sound generator
//	Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------

#ifndef PSG_H
#define PSG_H

#include <stdint.h>

class PSG
{
public:
	enum
	{
		noisetablesize = 1 << 11,	// ←メモリ使用量を減らしたいなら減らして
		toneshift = 24,
		envshift = 22,
		noiseshift = 14,
		oversampling = 2,		// ← 音質より速度が優先なら減らすといいかも
	};

public:
	PSG();
	~PSG();

	void Mix(int16_t * dest, int nsamples);
	void SetClock(int clock, int rate);
	
	void SetVolume(int vol);
	void SetChannelMask(int c);
	
	void Reset();
	void SetReg(uint32_t regnum, uint8_t data);
	uint32_t GetReg(uint32_t regnum) { return reg[regnum & 0x0f]; }

protected:
	void MakeNoiseTable();
	void MakeEnvelopTable();
	static void StoreSample(int16_t & dest, int32_t data);
	
	uint8_t reg[16];

	const uint32_t* envelop;
	uint32_t olevel[3];
	uint32_t scount[3], speriod[3];
	uint32_t ecount, eperiod;
	uint32_t ncount, nperiod;
	uint32_t tperiodbase;
	uint32_t eperiodbase;
	uint32_t nperiodbase;
	int volume;
	int mask;

	static uint32_t enveloptable[16][64];
	static uint32_t noisetable[noisetablesize];
	static int EmitTable[32];
};

#endif // PSG_H
