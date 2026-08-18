#ifndef __PCSXR_THREADS_H__
#define __PCSXR_THREADS_H__

/* [XBOX360] Implementacion de las primitivas de hilo que espera el
 * cdrom-async.c de pcsx_rearmed (USE_ASYNC_CDROM), mapeadas a Win32/XDK.
 * API identica a la del frontend/pcsxr-threads.h de pcsx_rearmed (rama
 * no-C11). Implementacion en pcsxr-threads.c. */

#ifdef __cplusplus
extern "C" {
#endif

enum pcsxr_thread_type
{
	PCSXRT_CDR = 0,
	PCSXRT_DRC,
	PCSXRT_GPU,
	PCSXRT_SPU,
	PCSXRT_COUNT // must be last
};

extern int pcsxr_sthread_core_count;

void pcsxr_sthread_init(void);

struct sthread;
struct slock;
struct scond;
typedef struct sthread sthread_t;
typedef struct slock slock_t;
typedef struct scond scond_t;

#define STRHEAD_RET_TYPE void
#define STRHEAD_RETURN()

struct sthread *pcsxr_sthread_create(void (*thread_func)(void*),
	enum pcsxr_thread_type type);
void sthread_join(struct sthread *thread);

struct slock *slock_new(void);
void slock_free(struct slock *lock);
void slock_lock(struct slock *lock);
void slock_unlock(struct slock *lock);

struct scond *scond_new(void);
void scond_free(struct scond *cond);
void scond_wait(struct scond *cond, struct slock *lock);
void scond_signal(struct scond *cond);

#ifdef __cplusplus
}
#endif

#endif // __PCSXR_THREADS_H__
