// Four-way SH3 block-cache tag probe. Keep first-match and miss-only RR rules.
#ifndef FBNEO_SH3_DRC_LOOKUP_H
#define FBNEO_SH3_DRC_LOOKUP_H

#if defined(_MSC_VER)
#define SH3_LOOKUP_INLINE __forceinline
#else
#define SH3_LOOKUP_INLINE inline
#endif

static SH3_LOOKUP_INLINE unsigned sh3_drc_lookup4(const UINT32 (&tag)[4],
 unsigned &next, UINT32 pc)
{
 // The array reference fixes this specialization to exactly four ways.
 // Zero/duplicate tags still select the first match, as the original loop did.
 if(tag[0]==pc)return 0;
 if(tag[1]==pc)return 1;
 if(tag[2]==pc)return 2;
 if(tag[3]==pc)return 3;
 return next++ & 3u;
}
#undef SH3_LOOKUP_INLINE
#endif
