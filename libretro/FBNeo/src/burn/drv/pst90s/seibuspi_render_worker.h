// A persistent worker for Seibu SPI drawing. Every submitted job is joined
// before returning a frame, changing emulated state, or releasing video RAM.
#ifndef FBNEO_SEIBUSPI_RENDER_WORKER_H
#define FBNEO_SEIBUSPI_RENDER_WORKER_H

#ifdef _XBOX
#include <xtl.h>

class SpiRenderWorker {
	HANDLE thread, ready, complete;
	bool quitting, pending;
	INT32 top, bottom;
	void (*draw)(INT32, INT32);

	static DWORD WINAPI run(void *data) {
		SpiRenderWorker *worker = (SpiRenderWorker *)data;
		// Salvia's emulation thread uses hardware thread 1 (physical core 0).
		// Use core 1; leave core 2 to the frontend's audio and I/O threads.
		XSetThreadProcessor(GetCurrentThread(), 2);
		for (;;) {
			WaitForSingleObject(worker->ready, INFINITE);
			if (worker->quitting) return 0;
			worker->draw(worker->top, worker->bottom);
			SetEvent(worker->complete);
		}
	}

public:
	SpiRenderWorker() : thread(NULL), ready(NULL), complete(NULL), quitting(false), pending(false) {}
	bool init(void (*callback)(INT32, INT32)) {
		draw = callback;
		quitting = pending = false;
		ready = CreateEvent(NULL, FALSE, FALSE, NULL);
		complete = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ready && complete) thread = CreateThread(NULL, 64 * 1024, run, this, 0, NULL);
		if (thread) return true;
		if (ready) CloseHandle(ready);
		if (complete) CloseHandle(complete);
		ready = complete = NULL;
		return false;
	}
	bool start(INT32 first, INT32 last) {
		if (!thread) return false;
		top = first; bottom = last;
		pending = true;
		SetEvent(ready);
		return true;
	}
	void finish() {
		if (pending) {
			WaitForSingleObject(complete, INFINITE);
			pending = false;
		}
	}
	void exit() {
		if (!thread) return;
		finish();
		quitting = true;
		SetEvent(ready);
		WaitForSingleObject(thread, INFINITE);
		CloseHandle(thread);
		CloseHandle(ready);
		CloseHandle(complete);
		thread = ready = complete = NULL;
	}
};

#elif defined(FBNEO_SPI_RENDER_TEST) && FBNEO_SPI_RENDER_TEST == 2
// Host-only backend for testing the same job boundaries against the original
// renderer. This does not enable threading in normal desktop core builds.
#include <pthread.h>

class SpiRenderWorker {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t ready, complete;
	bool active, quitting, work, done;
	INT32 top, bottom;
	void (*draw)(INT32, INT32);

	static void *run(void *data) {
		SpiRenderWorker *worker = (SpiRenderWorker *)data;
		pthread_mutex_lock(&worker->mutex);
		for (;;) {
			while (!worker->work && !worker->quitting) pthread_cond_wait(&worker->ready, &worker->mutex);
			if (worker->quitting) break;
			INT32 first = worker->top, last = worker->bottom;
			worker->work = false;
			pthread_mutex_unlock(&worker->mutex);
			worker->draw(first, last);
			pthread_mutex_lock(&worker->mutex);
			worker->done = true;
			pthread_cond_signal(&worker->complete);
		}
		pthread_mutex_unlock(&worker->mutex);
		return NULL;
	}

public:
	SpiRenderWorker() : active(false) {}
	bool init(void (*callback)(INT32, INT32)) {
		draw = callback;
		quitting = work = false;
		done = true;
		if (pthread_mutex_init(&mutex, NULL)) return false;
		if (pthread_cond_init(&ready, NULL)) { pthread_mutex_destroy(&mutex); return false; }
		if (pthread_cond_init(&complete, NULL)) {
			pthread_cond_destroy(&ready); pthread_mutex_destroy(&mutex); return false;
		}
		if (pthread_create(&thread, NULL, run, this)) {
			pthread_cond_destroy(&complete); pthread_cond_destroy(&ready); pthread_mutex_destroy(&mutex); return false;
		}
		active = true;
		return true;
	}
	bool start(INT32 first, INT32 last) {
		if (!active) return false;
		pthread_mutex_lock(&mutex);
		top = first; bottom = last;
		done = false; work = true;
		pthread_cond_signal(&ready);
		pthread_mutex_unlock(&mutex);
		return true;
	}
	void finish() {
		if (!active) return;
		pthread_mutex_lock(&mutex);
		while (!done) pthread_cond_wait(&complete, &mutex);
		pthread_mutex_unlock(&mutex);
	}
	void exit() {
		if (!active) return;
		finish();
		pthread_mutex_lock(&mutex);
		quitting = true;
		pthread_cond_signal(&ready);
		pthread_mutex_unlock(&mutex);
		pthread_join(thread, NULL);
		pthread_cond_destroy(&complete);
		pthread_cond_destroy(&ready);
		pthread_mutex_destroy(&mutex);
		active = false;
	}
};

#else
class SpiRenderWorker {
public:
	bool init(void (*)(INT32, INT32)) { return false; }
	bool start(INT32, INT32) { return false; }
	void finish() {}
	void exit() {}
};
#endif
#endif
