#include "burner.h"
#include "ThreadLock.h"

ThreadLock::ThreadLock()
{
	InitializeCriticalSection(&lock);
	LockCount = 0;
	LockThreadId = 0;
}

ThreadLock::~ThreadLock()
{
    DeleteCriticalSection(&lock);
}

void ThreadLock::Lock()
{
	EnterCriticalSection(&lock);
}


void ThreadLock::Unlock()
{
	LeaveCriticalSection(&lock);
}