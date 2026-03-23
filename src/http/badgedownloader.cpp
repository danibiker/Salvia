#include "badgedownloader.h"

void BadgeDownloader::start() {
    if (!running) {
        running = true;
        hThread = CreateThread(NULL, 0, thread_func, this, CREATE_SUSPENDED, NULL);
        if (hThread) {
			#ifdef _XBOX
			// Forzar la ejecución en el núcleo especificado
			XSetThreadProcessor(hThread, CPU_THREAD); 
			#endif
			// Bajar prioridad para no afectar el rendimiento del juego/emulador
			SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
			// Arrancar el hilo que estaba suspendido
			ResumeThread(hThread);
		}
    }
}

void BadgeDownloader::stop() {
    if (!running) return;

    running = false; // Señalizamos al hilo que debe salir del bucle while
    
    if (hThread != NULL) {
        // Esperamos un máximo de 2 segundos (2000 ms)
        DWORD result = WaitForSingleObject(hThread, 2000);

        if (result == WAIT_TIMEOUT) {
            // El hilo se quedó bloqueado (probablemente en un socket de red)
            // En Xbox 360, podrías optar por TerminateThread si es crítico, 
            // aunque lo ideal es que tu función de red tenga su propio timeout.
            LOG_DEBUG("BadgeDownloader: Timeout al detener el hilo. Forzando cierre.");
            // Opcional: TerminateThread(hThread, 0); // Solo si es estrictamente necesario
        }

        CloseHandle(hThread);
        hThread = NULL;

        // Limpiamos la cola bajo el mutex para evitar fugas
        ScopedLock lock(mutex);
        colaDescarga.clear();
    }
}

void BadgeDownloader::add_to_deque(AchievementState &achievement, int w, int h) {
	ScopedLock lock(mutex); // Bloquea al entrar
    
    if (achievement.badge != NULL) { 
        return; 
    }

    for(size_t i = 0; i < colaDescarga.size(); ++i) {
        if(colaDescarga[i].achievement == &achievement) {
            return;
        }
    }

    BadgeDownloadTask task;
    task.achievement = &achievement;
    task.w = w;
    task.h = h;

    colaDescarga.push_back(task);
}

DWORD WINAPI BadgeDownloader::thread_func(LPVOID data) {
    BadgeDownloader* self = (BadgeDownloader*)data;
    
    while (self->running) {
        BadgeDownloadTask currentTask;
        bool hasTask = false;

        // --- BLOQUE DE PROTECCIÓN CORTO ---
        {
            ScopedLock lock(self->mutex); 
            if (!self->colaDescarga.empty()) {
                currentTask = self->colaDescarga.front();
                self->colaDescarga.pop_front();
                hasTask = true;
            }
        } // <--- EL MUTEX SE LIBERA JUSTO AQUÍ AUTOMÁTICAMENTE

        // --- PROCESAMIENTO FUERA DEL MUTEX ---
        if (hasTask) {
            // Ahora la descarga ocurre sin bloquear a nadie más
            Achievements::instance()->download_and_cache_image(currentTask.achievement, currentTask.w, currentTask.h, true);
        } else {
            // Dormimos sin bloquear el mutex
            Sleep(100); 
        }
    }
    return 0;
}