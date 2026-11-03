// Included inside Sh3Ppc after MAX_INSNS and Sh3PpcState are declared.
// This is translation metadata, never guest state or a serialized structure.
#ifndef FBNEO_SH3_DRC_BLOCK_H
#define FBNEO_SH3_DRC_BLOCK_H

struct Block {
 UINT32 pc;
 const UINT16 *source;
 int (*entry)(Sh3PpcState*);
 UINT16 words, cycles;
 bool check_read_map;
 UINT16 original[MAX_INSNS + 1];
};

// Keep frequently used metadata before the variable-length validated snapshot.
// The 32-bit Xbox/PPC record must retain its old allocation size. The host-only
// control-flow tests also include this declaration under their native ABI.
typedef char Block32BitSizeUnchanged[
 (sizeof(void*) != 4 || sizeof(Block) == 84) ? 1 : -1];
typedef char Block32BitHotOffsets[
 (sizeof(void*) != 4 ||
  (offsetof(Block,pc) == 0 && offsetof(Block,source) == 4 &&
   offsetof(Block,entry) == 8 && offsetof(Block,words) == 12 &&
   offsetof(Block,cycles) == 14 && offsetof(Block,check_read_map) == 16 &&
   offsetof(Block,original) == 18)) ? 1 : -1];
typedef char BlockSnapshotHalfwordAligned[
 (offsetof(Block,original) % sizeof(UINT16) == 0) ? 1 : -1];
#endif
