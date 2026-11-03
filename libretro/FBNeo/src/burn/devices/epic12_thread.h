// CV1000 keeps the blit list in order: uploads and draws can read back earlier
// results from the same VRAM. Run that list on one persistent physical core
// while the main core emulates the SH3; do not split dependent commands.
#if defined(_XBOX) || defined(FBNEO_RENDER_THREADS_TEST)
#include "render_worker.h"

static void epic12_thread_job(INT32, INT32, INT32);

struct epic12_thread {
	BurnRenderWorker worker;
	void (*our_callback)();
	INT32 startup_frame;
	bool available, enabled, pending;

	void init(void (*callback)()) {
		our_callback = callback;
		startup_frame = 0;
		enabled = true;
		pending = false;
		available = worker.init(epic12_thread_job, 1);
	}

	void notify_wait() {
		if (!pending) { worker.finish(); return; }
		worker.finish();
		pending=false;
	}
	void exit() { notify_wait(); worker.exit(); available = false; pending=false; }
	void reset() { notify_wait(); startup_frame = 180; }
	void scan() { notify_wait(); SCAN_VAR(startup_frame); }

	void set_threading(INT32 value) {
		// A change to single-core mode must finish the previous list first.
		bool requested = value && nBurnRenderCores > 1;
		if (enabled != requested) notify_wait();
		enabled = requested;
	}

	void notify() {
		if (startup_frame > 0) {
			startup_frame--;
			our_callback();
		} else if (available && enabled) {
			worker.start(0, 0);
			pending = true;
		} else {
			our_callback();
		}
	}
};

static epic12_thread thready;

static void epic12_thread_job(INT32, INT32, INT32)
{
	thready.our_callback();
}
#else
#include "thready.h"
#endif
