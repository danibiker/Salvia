#include "psxcommon.h"

/* [XBOX360] Lectura de CD asincrona: el hilo lector (worker) es el UNICO dueno
 * de la imagen (chd_img); el hilo de emulacion NUNCA lee/descomprime. En un
 * cache-miss de gameplay, la IRQ del CD se re-agenda (drive-busy) via cdra_peek
 * en vez de bloquear el hilo emu descomprimiendo un hunk (causaba stalls de
 * 15-340ms en cada seek). Requiere el worker activo (cdrom_prefetch ON). */

#ifdef __cplusplus
extern "C" {
#endif

struct CdrStat;

#ifdef HAVE_CDROM
void *rcdrom_open(const char *name, u32 *total_lba, u32 *have_sub);
void rcdrom_close(void *stream);
int  rcdrom_getTN(void *stream, u8 *tn);
int  rcdrom_getTD(void *stream, u32 total_lba, u8 track, u8 *rt);
int  rcdrom_getStatus(void *stream, struct CdrStat *stat);
int  rcdrom_readSector(void *stream, unsigned int lba, void *b);
int  rcdrom_readSub(void *stream, unsigned int lba, void *b);
int  rcdrom_isMediaInserted(void *stream);
#endif

int  cdra_init(void);
void cdra_shutdown(void);
int  cdra_open(void);
void cdra_close(void);
int  cdra_getTN(unsigned char *tn);
int  cdra_getTD(int track, unsigned char *rt);
int  cdra_getStatus(struct CdrStat *stat);
int  cdra_readTrack(const unsigned char *time);
int  cdra_readCDDA(const unsigned char *time, void *buffer);
int  cdra_readSub(const unsigned char *time, void *buffer);
int  cdra_prefetch(unsigned char m, unsigned char s, unsigned char f, int is_cdda);
/* [XBOX360] Peek no bloqueante: devuelve 1 si el sector ya esta en la cache del
 * worker (listo para leer sin stall), 0 si no (y en ese caso lanza la peticion
 * al worker). El llamador (cdrReadInterrupt/cdrPlayReadInterrupt) re-agenda la
 * IRQ del CD cuando devuelve 0. */
int  cdra_peek(const unsigned char *time, int is_cdda);

int  cdra_is_physical(void);
int  cdra_check_eject(int *inserted);
void cdra_stop_thread(void);
void cdra_set_buf_count(int count);
int  cdra_get_buf_count(void);
int  cdra_get_buf_cached_approx(void);

void *cdra_getBuffer(void);

#ifdef __cplusplus
}
#endif
