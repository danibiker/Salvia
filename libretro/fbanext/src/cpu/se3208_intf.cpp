#include "burnint.h"
#include "se3208_intf.h"

//#define DEBUG

#define MAX_MEMORY	0x08000000 // max should be (1<<32)! 32-bit!
#define MAX_MASK	0x07ffffff // max-1
#define PAGE_SIZE	0x00001000 // 1000 seems good...
#define PAGE_COUNT	(MAX_MEMORY/PAGE_SIZE)
#define PAGE_SHIFT	12	// 0x1000 -> 12 bits
#define PAGE_BYTE_AND	0x00fff	// 0x1000 - 1 (byte align)
#define PAGE_WORD_AND	0x00ffe	// 0x1000 - 2 (word align)
#define PAGE_LONG_AND	0x00ffc // 0x1000 - 4 (ignore last 4 bytes, long align)

#define READ	0
#define WRITE	1
#define FETCH	2

static UINT8 **membase[3]; // 0 read, 1, write, 2 opcode

static void (*pWriteLongHandler)(UINT32, UINT32  ) = NULL;
static void (*pWriteWordHandler)(UINT32, UINT16  ) = NULL;
static void (*pWriteByteHandler)(UINT32, UINT8 ) = NULL;

static UINT32 (*pReadLongHandler)(UINT32) = NULL;
static UINT16 (*pReadWordHandler)(UINT32) = NULL;
static UINT8  (*pReadByteHandler)(UINT32) = NULL;

void se3208Open(INT32 /*num*/)
{

}

void se3208Close()
{

}

INT32 se3208GetActive()
{
	return 0; // only one cpu supported
}

static cpu_core_config se3208CheatCpuConfig =
{
	se3208Open,
	se3208Close,
	se3208_read_byte,
	se3208_write_rom_byte,
	se3208GetActive,
	se3208TotalCycles,
	se3208NewFrame,
	se3208Run,
	se3208RunEnd,
	se3208Reset,
	MAX_MEMORY,
	0
};

void se3208Init(INT32 /*num*/) // only one cpu supported
{
	void se3208Init();

	se3208Init();

	for (INT32 i = 0; i < 3; i++) {
		membase[i] = (UINT8**)malloc(PAGE_COUNT * sizeof(UINT8**));
		memset (membase[i], 0, PAGE_COUNT * sizeof(UINT8**));
	}

	pWriteLongHandler = NULL;
	pWriteWordHandler = NULL;
	pWriteByteHandler = NULL;
	pReadLongHandler = NULL;
	pReadWordHandler = NULL;
	pReadByteHandler = NULL;

	CpuCheatRegister(0, &se3208CheatCpuConfig);
}

void se3208MapMemory(UINT8 *src, INT32 start, INT32 finish, INT32 type)
{
	UINT32 len = (finish-start) >> PAGE_SHIFT;

	for (UINT32 i = 0; i < len+1; i++)
	{
		UINT32 offset = i + (start >> PAGE_SHIFT);
		if (type & (1 <<  READ)) membase[ READ][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << WRITE)) membase[WRITE][offset] = src + (i << PAGE_SHIFT);
		if (type & (1 << FETCH)) membase[FETCH][offset] = src + (i << PAGE_SHIFT);
	}
}

void se3208SetWriteByteHandler(void (*write)(UINT32, UINT8))
{
	pWriteByteHandler = write;
}

void se3208SetWriteWordHandler(void (*write)(UINT32, UINT16))
{
	pWriteWordHandler = write;
}

void se3208SetWriteLongHandler(void (*write)(UINT32, UINT32))
{
	pWriteLongHandler = write;
}

void se3208SetReadByteHandler(UINT8 (*read)(UINT32))
{
	pReadByteHandler = read;
}

void se3208SetReadWordHandler(UINT16 (*read)(UINT32))
{
	pReadWordHandler = read;
}

void se3208SetReadLongHandler(UINT32 (*read)(UINT32))
{
	pReadLongHandler = read;
}

void se3208_write_byte(UINT32 addr, UINT8 data)
{
	addr &= MAX_MASK;

#ifdef DEBUG
	bprintf (0, _T("WB: %8.8x, %2.2x\n"), addr, data);
#endif

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
		return;
	}

	if (pWriteByteHandler) {
		pWriteByteHandler(addr, data);
	}
}

void se3208_write_word(UINT32 addr, UINT16 data)
{
	addr &= MAX_MASK;

#ifdef DEBUG
	bprintf (0, _T("WW: %8.8x, %4.4x\n"), addr, data);
#endif

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		*((UINT16*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (pWriteWordHandler) {
		pWriteWordHandler(addr, data);
	}
}

void se3208_write_dword(UINT32 addr, UINT32 data)
{
	addr &= MAX_MASK;

#ifdef DEBUG
	bprintf (0, _T("WL: %8.8x, %8.8x\n"), addr, data);
#endif

	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		*((UINT32*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND))) = BURN_ENDIAN_SWAP_INT32(data);
		return;
	}

	if (pWriteLongHandler) {
		pWriteLongHandler(addr, data);
	}
}

UINT8 se3208_read_byte(UINT32 addr)
{
	addr &= MAX_MASK;

#ifdef DEBUG
	bprintf (0, _T("RB: %8.8x\n"), addr);
#endif

	if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
		return membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND];
	}

	if (pReadByteHandler) {
		return pReadByteHandler(addr);
	}

	return 0;
}

UINT16 se3208_read_word(UINT32 addr)
{
	addr &= MAX_MASK;

#ifdef DEBUG
	bprintf (0, _T("RW: %8.8x\n"), addr);
#endif

	if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(membase[ READ][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))));
	}

	if (pReadByteHandler) {
		return pReadWordHandler(addr);
	}

	return 0;
}

UINT32 se3208_read_dword(UINT32 addr)
{
	addr &= MAX_MASK;

#ifdef DEBUG
	bprintf (0, _T("RL: %8.8x\n"), addr);
#endif

	if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT32(*((UINT32*)(membase[ READ][addr >> PAGE_SHIFT] + (addr & PAGE_LONG_AND))));
	}

	if (pReadLongHandler) {
		return pReadLongHandler(addr);
	}

	return 0;
}

UINT32 se3208_fetch_word(UINT32 addr)
{
#ifdef DEBUG
	bprintf (0, _T("FL: %8.8x\n"), addr);
#endif

	addr &= MAX_MASK;

	if (membase[FETCH][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(membase[FETCH][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))));
	}

	// good enough for now...
	if (pReadWordHandler) {
		return pReadWordHandler(addr);
	}

	return 0;
}

void se3208SetIRQLine(INT32 line, INT32 state)
{
	bprintf (0, _T("interrupt %d, %d\n"), line, state);
	void se3208_set_irq_line(int line,int state);

	if (state == SE3208_IRQSTATUS_NONE || state == SE3208_IRQSTATUS_ACK) {
		se3208_set_irq_line(line, state);
	} else if (SE3208_IRQSTATUS_AUTO) {
		se3208_set_irq_line(line, SE3208_IRQSTATUS_ACK);
		se3208Run(10);
		se3208_set_irq_line(line, SE3208_IRQSTATUS_NONE);
	}
}

// For cheats/etc

void se3208_write_rom_byte(UINT32 addr, UINT8 data)
{
	addr &= MAX_MASK;

	// write to rom & ram
	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
	}

	if (membase[READ][addr >> PAGE_SHIFT] != NULL) {
		membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
	}

	if (pWriteByteHandler) {
		pWriteByteHandler(addr, data);
	}
}
