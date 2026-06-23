
void se3208NewFrame();
INT32 se3208GetActive();
INT32 se3208TotalCycles();
void se3208RunEnd();

void se3208Init(INT32 /*num*/);
void se3208SetIRQCallback(INT32 (*irqcallback)(INT32));
void se3208Open(INT32 /*num*/);
void se3208Reset();
INT32 se3208Run(INT32 cycles);
void se3208Close();
void se3208Exit();

#define SE3208_READ	1
#define SE3208_WRITE	2
#define SE3208_FETCH	4

#define SE3208_ROM	(SE3208_READ | SE3208_FETCH)
#define SE3208_RAM	(SE3208_ROM | SE3208_WRITE)

void se3208MapMemory(UINT8 *src, INT32 start, INT32 finish, INT32 type);

void se3208SetWriteByteHandler(void (*write)(UINT32, UINT8));
void se3208SetWriteWordHandler(void (*write)(UINT32, UINT16));
void se3208SetWriteLongHandler(void (*write)(UINT32, UINT32));

void se3208SetReadByteHandler(UINT8 (*read)(UINT32));
void se3208SetReadWordHandler(UINT16 (*read)(UINT32));
void se3208SetReadLongHandler(UINT32 (*read)(UINT32));

#define SE3208_INT_LINE		0
#define SE3208_NMI_LINE		0x20

#define SE3208_IRQSTATUS_NONE	0
#define SE3208_IRQSTATUS_ACK	1
#define SE3208_IRQSTATUS_AUTO	2

void se3208SetIRQLine(INT32 line, INT32 state);

void se3208_write_byte(UINT32 addr, UINT8 data);
void se3208_write_word(UINT32 addr, UINT16 data);
void se3208_write_dword(UINT32 addr, UINT32 data);

UINT8 se3208_read_byte(UINT32 addr);
UINT16 se3208_read_word(UINT32 addr);
UINT32 se3208_read_dword(UINT32 addr);
UINT32 se3208_fetch_word(UINT32 addr);

void se3208_write_rom_byte(UINT32 addr, UINT8 data);

UINT32 se3208GetPC(INT32 /*cpu*/);

