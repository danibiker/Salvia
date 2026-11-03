// A persistent worker for software drawing. Every submitted job is joined
// before returning a frame, changing emulated state, or releasing video RAM.
#ifndef FBNEO_RENDER_WORKER_H
#define FBNEO_RENDER_WORKER_H

// Keep frequently written contexts on separate Xbox 360 cache lines.
#if defined(_MSC_VER)
#define BURN_RENDER_ALIGN __declspec(align(128))
#elif defined(__GNUC__)
#define BURN_RENDER_ALIGN __attribute__((aligned(128)))
#else
#define BURN_RENDER_ALIGN
#endif

#ifdef _XBOX
#include <xtl.h>

class BURN_RENDER_ALIGN BurnRenderWorker {
	HANDLE thread, ready, complete;
	bool quitting, pending;
	INT32 top, bottom, index;
	void (*draw)(INT32, INT32, INT32);

	static DWORD WINAPI run(void *data) {
		BurnRenderWorker *worker = (BurnRenderWorker *)data;
		// Salvia's emulation thread uses hardware thread 1 (physical core 0).
		// Workers use core 1 and, optionally, core 2.
		XSetThreadProcessor(GetCurrentThread(), worker->index * 2);
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		for (;;) {
			WaitForSingleObject(worker->ready, INFINITE);
			if (worker->quitting) return 0;
			worker->draw(worker->top, worker->bottom, worker->index);
			SetEvent(worker->complete);
		}
	}

public:
	BurnRenderWorker() : thread(NULL), ready(NULL), complete(NULL), quitting(false), pending(false) {}
	bool init(void (*callback)(INT32, INT32, INT32), INT32 workerIndex) {
		index = workerIndex;
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

#elif defined(FBNEO_RENDER_THREADS_TEST)
// Host-only backend for testing the same job boundaries against the original
// renderer. This does not enable threading in normal desktop core builds.
#include <pthread.h>

class BURN_RENDER_ALIGN BurnRenderWorker {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t ready, complete;
	bool active, quitting, work, done;
	INT32 top, bottom, index;
	void (*draw)(INT32, INT32, INT32);

	static void *run(void *data) {
		BurnRenderWorker *worker = (BurnRenderWorker *)data;
		pthread_mutex_lock(&worker->mutex);
		for (;;) {
			while (!worker->work && !worker->quitting) pthread_cond_wait(&worker->ready, &worker->mutex);
			if (worker->quitting) break;
			INT32 first = worker->top, last = worker->bottom;
			worker->work = false;
			pthread_mutex_unlock(&worker->mutex);
			worker->draw(first, last, worker->index);
			pthread_mutex_lock(&worker->mutex);
			worker->done = true;
			pthread_cond_signal(&worker->complete);
		}
		pthread_mutex_unlock(&worker->mutex);
		return NULL;
	}

public:
	BurnRenderWorker() : active(false) {}
	bool init(void (*callback)(INT32, INT32, INT32), INT32 workerIndex) {
		index = workerIndex;
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
class BurnRenderWorker {
public:
	bool init(void (*)(INT32, INT32, INT32), INT32) { return false; }
	bool start(INT32, INT32) { return false; }
	void finish() {}
	void exit() {}
};
#endif

extern INT32 nBurnRenderCores;

// Main thread is context 0; persistent workers are contexts 1 and 2. Counts
// change only between frames, and every job is joined before returning.
class BurnRenderPool {
	BurnRenderWorker workers[2];
	void (*draw)(INT32, INT32, INT32);
	INT32 count;
public:
	BurnRenderPool() : draw(NULL), count(0) {}
	void init(void (*callback)(INT32, INT32, INT32)) { draw = callback; }
	void exit() { workers[0].exit(); workers[1].exit(); count = 0; }
	void render(INT32 height, bool enabled = true) {
		INT32 requested = 1;
#if defined(_XBOX) || defined(FBNEO_RENDER_THREADS_TEST)
		if (enabled && nBurnRenderCores >= 1 && nBurnRenderCores <= 3) requested = nBurnRenderCores;
#endif
		if (count != requested) {
			exit();
			count = requested;
			for (INT32 i = 1; i < count; i++) workers[i - 1].init(draw, i);
		}
		INT32 top = 0;
		for (INT32 i = 1; i < count; i++) {
			INT32 bottom = ((height * i / count) + 15) & ~15;
			if (bottom > height) bottom = height;
			if (!workers[i - 1].start(top, bottom)) draw(top, bottom, i);
			top = bottom;
		}
		draw(top, height, 0);
		for (INT32 i = 1; i < count; i++) workers[i - 1].finish();
	}
};
#endif
