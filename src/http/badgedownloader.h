#include <vector>
#include <string>
#include <SDL_thread.h>
#include <SDL.h>

struct BadgeDownloadTask {
    std::string url;
    SDL_Surface** targetBadge; // Puntero al badge de AchievementState
	int w,h;
};

class BadgeDownloader {
public:
    static BadgeDownloader& instance() {
        static BadgeDownloader _instance;
        return _instance;
    }

    void start();
    void stop();
    void add_to_queue(const std::string& url, SDL_Surface** target, int w, int h);

private:
    BadgeDownloader() : thread(NULL), running(false) {
        queue_mutex = SDL_CreateMutex();
    }

	BadgeDownloader::~BadgeDownloader() {
		// 1. Asegurarnos de que el hilo se detuvo
		stop(); 

		// 2. Destruir el mutex
		if (this->queue_mutex != NULL) {
			SDL_DestroyMutex(this->queue_mutex);
			this->queue_mutex = NULL;
		}
	}
    
    static int thread_func(void* data);
    
    SDL_Thread* thread;
    SDL_mutex* queue_mutex;
    std::vector<BadgeDownloadTask> queue;
    bool running;
};