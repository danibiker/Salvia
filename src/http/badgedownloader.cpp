#include "badgedownloader.h"

void BadgeDownloader::start() {
    if (!running) {
        running = true;
        hThread = CreateThread(NULL, 0, thread_func, this, CREATE_SUSPENDED, NULL);
        if (hThread) {
			Constant::setup_and_run_thread(hThread, CPU_THREAD);
		}
    }
}

void BadgeDownloader::stop() {
    if (!running) return;

    running = false; 
    if (hThread != NULL) {
        // Esperamos a que el hilo termine limpiamente
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = NULL;

        EnterCriticalSection(&queue_mutex);
        queue.clear();
        LeaveCriticalSection(&queue_mutex);
    }
}

void BadgeDownloader::add_to_queue(AchievementState &achievement, int w, int h) {
    EnterCriticalSection(&queue_mutex);
    
    if (achievement.badge != NULL) { 
        LeaveCriticalSection(&queue_mutex);
        return; 
    }

    for(size_t i = 0; i < queue.size(); ++i) {
        if(queue[i].achievement == &achievement) {
            LeaveCriticalSection(&queue_mutex);
            return;
        }
    }

    BadgeDownloadTask task;
    task.achievement = &achievement;
    task.w = w;
    task.h = h;
    
    queue.push_back(task);
    LeaveCriticalSection(&queue_mutex);
}

DWORD WINAPI BadgeDownloader::thread_func(LPVOID data) {
    BadgeDownloader* self = (BadgeDownloader*)data;
    
    while (self->running) {
        BadgeDownloadTask currentTask;
        bool hasTask = false;

        EnterCriticalSection(&self->queue_mutex);
        if (!self->queue.empty()) {
            currentTask = self->queue.front();
            self->queue.erase(self->queue.begin());
            hasTask = true;
        }
        LeaveCriticalSection(&self->queue_mutex);

        if (hasTask) {
            Achievements::instance()->download_and_cache_image(currentTask.achievement, currentTask.w, currentTask.h, true);
        } else {
            Sleep(100); // Reemplazo de SDL_Delay
        }
    }
    return 0;
}