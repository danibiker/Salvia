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

class SafeDownloadQueue {
private:
    std::queue<DownloadTask> tasks;
    CRITICAL_SECTION cs;
    HANDLE hEvent;
    bool finished;

public:
    SafeDownloadQueue() : finished(false) {
        InitializeCriticalSection(&cs);
        hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    ~SafeDownloadQueue() {
        DeleteCriticalSection(&cs);
        CloseHandle(hEvent);
    }

    void push(DownloadTask task) {
        EnterCriticalSection(&cs);
        tasks.push(task);
        LeaveCriticalSection(&cs);
        SetEvent(hEvent); // Despierta al hilo de descarga
    }

    bool pop(DownloadTask& task) {
        while (true) {
            EnterCriticalSection(&cs);
            if (!tasks.empty()) {
                task = tasks.front();
                tasks.pop();
                LeaveCriticalSection(&cs);
                return true;
            }
            if (finished) {
                LeaveCriticalSection(&cs);
                return false;
            }
            LeaveCriticalSection(&cs);
            WaitForSingleObject(hEvent, INFINITE);
        }
    }

    void setFinished() {
        EnterCriticalSection(&cs);
        finished = true;
        LeaveCriticalSection(&cs);
        SetEvent(hEvent); // Despierta para terminar
    }
};