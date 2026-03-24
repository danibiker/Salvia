#pragma once

#ifdef _XBOX
#include <xtl.h>
#elif defined(WIN32)
#include <windows.h>
#endif

#include <queue>
#include <string>

struct DownloadTask {
    std::string url;
    std::string destPath;
    float downloadProgress; // Usamos valor por copia para evitar punteros volátiles
};

namespace safequeue{
	class ScopedLock {
		SDL_mutex* m;
	public:
		// Al crear el objeto en el stack, bloqueamos
		explicit ScopedLock(SDL_mutex* mutex) : m(mutex) {
			if(m) SDL_mutexP(m);
		}
		// Al salir del scope (llave de cierre), desbloqueamos
		~ScopedLock() {
			if(m) SDL_mutexV(m);
		}
	private:
		// Prohibimos copiar el lock para evitar errores lógicos
		ScopedLock(const ScopedLock&);
		ScopedLock& operator=(const ScopedLock&);
	};
};

class SafeDownloadQueue {
private:
    std::queue<DownloadTask> tasks;
    bool finished;
    SDL_mutex* mutex;
    SDL_cond* cond; // Variable de condición para notificar cambios

public:
    SafeDownloadQueue() : finished(false) {
        mutex = SDL_CreateMutex();
        cond = SDL_CreateCond();
    }

    ~SafeDownloadQueue() {
        SDL_DestroyCond(cond);
        SDL_DestroyMutex(mutex);
    }

    void push(DownloadTask task) {
        {
            safequeue::ScopedLock lock(mutex);
            tasks.push(task);
        }
        SDL_CondSignal(cond); // Despierta a un hilo que esté esperando en pop()
    }

    // Devuelve false solo si la cola está vacía Y setFinished() fue llamado
    bool pop(DownloadTask& task) {
        SDL_LockMutex(mutex);
        
        // Mientras la cola esté vacía y NO hayamos terminado, esperamos
        while (tasks.empty() && !finished) {
            SDL_CondWait(cond, mutex); // Libera el mutex y duerme el hilo
        }

        if (!tasks.empty()) {
            task = tasks.front();
            tasks.pop();
            SDL_UnlockMutex(mutex);
            return true;
        }

        SDL_UnlockMutex(mutex);
        return false; // Cola vacía y finished == true
    }

    void setFinished() {
        {
            safequeue::ScopedLock lock(mutex);
            finished = true;
        }
        SDL_CondBroadcast(cond); // Despierta a todos los hilos para que mueran
    }

    int size() {
        safequeue::ScopedLock lock(mutex);
        return (int)tasks.size();
    }
};