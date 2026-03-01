#include "badgedownloader.h"
#include "achievements.h" // Para usar tu función download_and_cache_image

void BadgeDownloader::start() {
    if (!running) {
        running = true;
        thread = SDL_CreateThread(thread_func, this);
    }
}

void BadgeDownloader::stop() {
    running = false; // El hilo verá esto y saldrá de su bucle
    if (thread != NULL) {
        SDL_WaitThread(thread, NULL); // Esperamos a que termine la descarga actual
        thread = NULL;
    }
}

void BadgeDownloader::add_to_queue(const std::string& url, SDL_Surface** target, int w, int h) {
    SDL_LockMutex(queue_mutex);
    
    // Evitar duplicados: Si ya está en cola o ya tiene imagen, no añadir
    if (*target != NULL) { 
        SDL_UnlockMutex(queue_mutex);
        return; 
    }
    BadgeDownloadTask task;
    task.url = url;
    task.targetBadge = target;
	task.w = w;
	task.h = h;
    queue.push_back(task);
    SDL_UnlockMutex(queue_mutex);
}

int BadgeDownloader::thread_func(void* data) {
    BadgeDownloader* self = (BadgeDownloader*)data;
    
    while (self->running) {
        BadgeDownloadTask currentTask;
        bool hasTask = false;

        SDL_LockMutex(self->queue_mutex);
        if (!self->queue.empty()) {
            currentTask = self->queue.front();
            self->queue.erase(self->queue.begin());
            hasTask = true;
        }
        SDL_UnlockMutex(self->queue_mutex);

        if (hasTask) {
            // Llamamos a tu función existente de descarga y escalado
            // Pasamos *targetBadge por referencia para que actualice el puntero original
            Achievements::instance()->download_and_cache_image(
                currentTask.url, *(currentTask.targetBadge), currentTask.w, currentTask.h
            );
        } else {
            SDL_Delay(100); // Descansar si no hay tareas
        }
    }
    return 0;
}