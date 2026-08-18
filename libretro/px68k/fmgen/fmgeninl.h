// ---------------------------------------------------------------------------
//	FM Sound Generator
//	Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//	$fmgen-Id: fmgeninl.h,v 1.26 2003/06/12 13:14:36 cisc Exp $

#ifndef FM_GEN_INL_H
#define FM_GEN_INL_H

// ---------------------------------------------------------------------------
//	定数その２
//	
#define FM_PI		3.14159265358979323846f

#define FM_SINEPRESIS	2	// EGとサイン波の精度の差  0(低)-2(高)


#define FM_OPSINBITS	10
#define FM_OPSINENTS	(1 << FM_OPSINBITS)

#define FM_EGCBITS	18		// eg の count のシフト値
#define FM_LFOCBITS	14

#ifdef FM_TUNEBUILD
 #define FM_PGBITS	2
 #define FM_RATIOBITS	0
#else
 #define FM_PGBITS	9		
 #define FM_RATIOBITS	7			// 8-12 くらいまで？
#endif

#define FM_EGBITS	16

namespace FM
{

//	キーオン
inline void Operator::KeyOn()
{
	if (!keyon_)
	{
		keyon_ = true;
		if (eg_phase_ == OFF || eg_phase_ == RELEASE)
		{
			ssg_phase_ = -1;
			ShiftPhase(ATTACK);
			EGUpdate();
			out_ = out2_ = 0;
			pg_count_ = 0;
		}
	}
}

//	キーオフ
inline void	Operator::KeyOff()
{
	if (keyon_)
	{
		keyon_ = false;
		ShiftPhase(RELEASE);
	}
}

//	オペレータは稼働中か？
inline int Operator::IsOn()
{
	return eg_phase_ - OFF;
}

//	Detune (0-7)
inline void Operator::SetDT(uint32_t dt)
{
	detune_ = dt * 0x20, param_changed_ = true;
}

//	DT2 (0-3)
inline void Operator::SetDT2(uint32_t dt2)
{
	detune2_ = dt2 & 3, param_changed_ = true;
}

//	Multiple (0-15)
inline void Operator::SetMULTI(uint32_t mul)	
{ 
	multiple_ = mul, param_changed_ = true;
}

//	Total Level (0-127) (0.75dB step)
inline void Operator::SetTL(uint32_t tl, bool csm)
{
	if (!csm)
		tl_ = tl, param_changed_ = true;
	tl_latch_ = tl;
}

//	Attack Rate (0-63)
inline void Operator::SetAR(uint32_t ar)
{
	ar_ = ar; 
	param_changed_ = true;
}

//	Decay Rate (0-63)
inline void Operator::SetDR(uint32_t dr)
{ 
	dr_ = dr; 
	param_changed_ = true;
}

//	Sustain Rate (0-63)
inline void Operator::SetSR(uint32_t sr)		
{ 
	sr_ = sr;
	param_changed_ = true;
}

//	Sustain Level (0-127)
inline void Operator::SetSL(uint32_t sl)		
{ 
	sl_ = sl; 
	param_changed_ = true;
}

//	Release Rate (0-63)
inline void Operator::SetRR(uint32_t rr)		
{ 
	rr_ = rr; 
	param_changed_ = true;
}

//	Keyscale (0-3)
inline void Operator::SetKS(uint32_t ks)		
{ 
	ks_ = ks; 
	param_changed_ = true; 
}

//	SSG-type Envelop (0-15)
inline void Operator::SetSSGEC(uint32_t ssgec)	
{ 
	if (ssgec & 8)
		ssg_type_ = ssgec; 
	else
		ssg_type_ = 0;
}

inline void Operator::SetAMON(bool amon)		
{ 
	amon_ = amon;  
	param_changed_ = true;
}

inline void Operator::Mute(bool mute)
{
	mute_ = mute;
	param_changed_ = true;
}

inline void Operator::SetMS(uint32_t ms)
{
	ms_ = ms;
	param_changed_ = true;
}

// ---------------------------------------------------------------------------
//	4-op Channel

//	オペレータの種類 (LFO) を設定
inline void Channel4::SetType(OpType type)
{
	for (int i=0; i<4; i++)
		op[i].type_ = type;
}

//	セルフ・フィードバックレートの設定 (0-7)
inline void Channel4::SetFB(uint32_t feedback)
{
	fb = fbtable[feedback];
}

//	OPNA 系 LFO の設定
inline void Channel4::SetMS(uint32_t ms)
{
	op[0].SetMS(ms);
	op[1].SetMS(ms);
	op[2].SetMS(ms);
	op[3].SetMS(ms);
}

//	チャンネル・マスク
inline void Channel4::Mute(bool m)
{
	for (int i=0; i<4; i++)
		op[i].Mute(m);
}

inline void Channel4::SetChip(Chip* chip)
{
	chip_ = chip;
	for (int i=0; i<4; i++)
		op[i].SetChip(chip);
}

inline void StoreSample(int16_t & dest, ISample data)
{
	dest = (int16_t) Limit(dest + data, 0x7fff, -0x8000);
}


// ---------------------------------------------------------------------------
//	AM のレベルを設定
inline void Chip::SetAML(uint32_t l)
{
	aml_ = l & (FM_LFOENTS - 1);
}

//	PM のレベルを設定
inline void Chip::SetPML(uint32_t l)
{
	pml_ = l & (FM_LFOENTS - 1);
}


}

#endif // FM_GEN_INL_H
