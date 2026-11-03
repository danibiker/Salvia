// Shared C++ dispatch control. Included after the SH3 PPC cache/compiler.
// This does not link native blocks or bypass validation of mutable guest code.
#ifndef FBNEO_SH3_DRC_DISPATCH_H
#define FBNEO_SH3_DRC_DISPATCH_H
#include "sh3_drc_source_check.h"
#include "sh3_drc_lookup.h"


// Keep the native loop in the caller: interpreter fallback should not pay a
// second dispatcher stack frame. This does not inline/link generated PPC blocks.
#if SALVIA_CV1K_SHADOW
// Fetch the copy that will answer the successor of this entry. The successor is
// the last one this pc was seen to run into, which the dispatcher recorded on
// the previous pass, so the fetch that would otherwise stall the next entry is
// issued a whole block execution early -- the only lead time long enough to
// cover a miss to main memory. A mispredicted pc fetches a line nobody reads;
// a prefetch decides nothing, so there is no way for it to be wrong.
static inline void sh3_shadow_touch(const UINT32 pc)
{
#if SALVIA_CV1K_SHADOW_TOUCH
 const unsigned index=((pc>>1)^(pc>>11)^(pc>>21))&(Sh3Ppc::CACHE_SETS-1);
 const char *line=(const char*)Sh3Ppc::shadow+((size_t)index*sizeof(Sh3Ppc::Shadow));
#if defined(_XBOX) || (defined(__GNUC__) && (defined(__powerpc__) || defined(__ppc__)))
 Sh3Ppc::sh3_touch_line(line);
#else
 // The host models no cache, so it only has to prove the address arithmetic
 // stays inside the table it is derived from.
 if((const char*)line<(const char*)Sh3Ppc::shadow ||
    (const char*)line>=(const char*)Sh3Ppc::shadow+Sh3Ppc::CACHE_SETS*(int)sizeof(Sh3Ppc::Shadow)) abort();
#endif
#else
 (void)pc;
#endif
}
#endif
#if defined(_MSC_VER) && defined(_XBOX)
#define SH3_DISPATCH_INLINE __forceinline
#elif defined(__GNUC__)
#define SH3_DISPATCH_INLINE inline __attribute__((always_inline))
#else
#define SH3_DISPATCH_INLINE inline
#endif

template<bool Chained> static SH3_DISPATCH_INLINE bool sh3_drc_dispatch_impl()
{
 using namespace Sh3Ppc;
 // Preserve allocate()'s failure precedence and cold initialization. On a
 // warm cache it only returns true, so avoid its out-of-line call here.
 if(m_sh4_icount<=0 || failed || (!blocks && !allocate())) {
  return false;
 }
 // Generated entries are leaf functions: they cannot release the cache or
 // replace its allocation. Keep the dispatcher stack alive across entries.
 do {
  UINT32 pc=m_pc, phys=pc&AM;
  unsigned index=((pc>>1)^(pc>>11)^(pc>>21))&(CACHE_SETS-1);
  const UINT8 *page=MemMapF[phys>>SH3_SHIFT];
  if((uintptr_t)page<SH3_MAXHANDLER || (phys&1)) {
   return false;
  }
  const UINT16 *source=(const UINT16*)(page+(phys&SH3_PAGEM));
  // What the shared tail below needs, from whichever record answered: the
  // shadow copy if it can serve this entry, the table record otherwise.
  int (*entry)(Sh3PpcState*);
  int cycles;
  unsigned words, reason=0, stores=0;
#if SALVIA_CV1K_SHADOW
  // The shadow copy for this pc, if the dispatcher has one and it still
  // describes the bytes: its own tag and fetch page, a sector generation that
  // says the code it names is still resident, and the snapshot comparison --
  // the same validation the record path makes, not a weaker one.
  const Shadow *copy=NULL;
  if(shadow) {
   const Shadow &cand=shadow[index];
   if(cand.pc==pc && cand.source==source && sector_epoch[cand.sector]==cand.epoch &&
      (!cand.check_read_map || MemMapR[phys>>SH3_SHIFT]==page) &&
      sh3_drc_source_equal(cand.original,source,cand.words)) copy=&cand;
  }
  if(copy) {
   entry=copy->entry; cycles=copy->cycles; words=copy->words;
   reason=copy->reason; stores=copy->stores;
  } else
#endif
  {
  Lookup &set=lookup[index];
  const unsigned way=sh3_drc_lookup4(set.tag,set.next,pc);
  Block &b=blocks[index*WAYS+way];
  // Recheck EVERY entry, including successors, aliases, DMA/cheat writes and
  // changed fetch/read mappings. No cached host entry bypasses these guards.
  const bool rebuild=(b.source!=source || b.pc!=pc || (b.check_read_map && MemMapR[phys>>SH3_SHIFT]!=page) ||
    !sh3_drc_source_equal(b.original,source,b.words));
  if(rebuild) {
   compile(b,pc,source);
   arena_note_slot(index*WAYS+way,b);
   set.tag[way]=pc;
  }
  entry=b.entry; cycles=b.cycles; words=b.words;
  reason=slot_sector?(unsigned)(slot_sector[index*WAYS+way]>>5):0u;
  stores=slot_stores?slot_stores[index*WAYS+way]:0u;
#if SALVIA_CV1K_SHADOW
  if(shadow && b.entry) {
   // The record now describes the bytes at this pc, so this is the moment to
   // take the copy the next entry to this pc will read instead of it.
   Shadow &dst=shadow[index];
   dst.pc=b.pc; dst.source=b.source; dst.entry=b.entry;
   dst.words=b.words; dst.cycles=b.cycles;
   dst.check_read_map=b.check_read_map?1:0;
   dst.sector=(UINT8)arena_sector_of(b.entry);
   dst.epoch=sector_epoch[dst.sector];
   dst.reason=(UINT8)reason; dst.stores=(UINT8)stores;
   memcpy(dst.original,b.original,(unsigned)b.words*2);
  }
#endif
  }
  if(!entry) {
   return false;
  }
  if(m_sh4_icount<cycles) {
   return false;
  }
  // A partial/guarded block requests one interpreter step at its updated PC,
  // even when earlier blocks in this call completed. Do not return true here.
#if SALVIA_CV1K_SHADOW
  if(shadow) sh3_shadow_touch(shadow[index].next_pc);
#endif
  const int completed=entry(&sh3_ppc_state);
#if SALVIA_CV1K_SHADOW
  if(shadow) {
   // The pc the next entry resolves is this pc's observed successor, and the
   // line is already in hand from the read above.
   shadow[index].next_pc=(UINT32)m_pc;
  }
#endif
  // A compiler-tagged idle/device MOV.L needs one real handler access, but
  // no opcode refetch or trip through the outer interpreter decoder.
  bool serviced=false;
  if((completed&3)==2) serviced=sh3_drc_service_movll((unsigned)completed>>2);
  if(!completed || ((completed&3)==2 && !serviced)) {
   return false;
  }
  // These are exactly the outer timerhack loop's eligibility checks. Return
  // to that loop for delay slots, IRQ handling, mode changes or exhausted time.
 } while(Chained && m_sh4_icount>0 && sh3_drc_enabled && !m_delay && !m_test_irq);
 return true;
}
template<bool Chained> static SH3_DISPATCH_INLINE bool sh3_drc_dispatch()
{
 return sh3_drc_dispatch_impl<Chained>();
}
#undef SH3_DISPATCH_INLINE
#endif
