#include <deque>
#include <string>

#ifdef _XBOX
	#include <xtl.h> // API de Xbox 360 (incluye funciones de Windows)
#else 
	#include <windows.h>
#endif

#include "achievements.h"

struct BadgeDownloadTask {
    AchievementState *achievement;
    int w, h;
    BadgeDownloadTask() : achievement(NULL), w(0), h(0) {}
};

class BadgeDownloader {
public:
    static BadgeDownloader& instance() {
        static BadgeDownloader _instance;
        return _instance;
    }

    void start();
    void stop();
    void add_to_deque(AchievementState &achievement, int w, int h);

private:
    BadgeDownloader() : hThread(NULL), running(false) {
        mutex = SDL_CreateMutex();
    }

    ~BadgeDownloader() {
        stop();
        SDL_DestroyMutex(mutex);
    }
    
    // El prototipo de función para CreateThread debe ser DWORD WINAPI
    static DWORD WINAPI thread_func(LPVOID data);
    
    HANDLE hThread;
    SDL_mutex *mutex;
    std::deque<BadgeDownloadTask> colaDescarga;
    bool running;
};