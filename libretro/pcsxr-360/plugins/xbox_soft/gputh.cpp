#include <xtl.h>
#include <queue>

#include "gpu.h"
#include "gputh.h"

#include "opti.h"

std::queue <work_t> qwork;

HANDLE ghMutex = NULL;

static HANDLE hThreadRun = NULL;

CRITICAL_SECTION g_CsQueue;

/**
* Thread qui tourne en permanence
*/
static DWORD WINAPI ThreadRunner(LPVOID param){
	work_t cur;
	BOOL found = FALSE;
	while(1){
		found = FALSE;

		if (qwork.size()==0) 
			Sleep(200);

		if(qwork.size())
		{
			EnterCriticalSection( &g_CsQueue );
			found=TRUE;
			cur  = qwork.front();
			qwork.pop();
			LeaveCriticalSection( &g_CsQueue );
		}

		if(found==TRUE)
		{
			cur.func(cur.args);
		}
	}
}



/**
* Certain probleme pour arrive lorsque que malloc et free ne sont pas sur le meme core
**/
std::queue <void *> list_free; //list des adresses a free

CRITICAL_SECTION g_free_cs;

HANDLE hThreadFree = NULL;

DWORD WINAPI ThreadRunnerFree(LPVOID param){
	void * addr = NULL;
	while(1){
		if (list_free.size()==0) 
			Sleep(200);

		if (list_free.size()){
			EnterCriticalSection(&g_free_cs);
			addr = list_free.front();
			free(addr);
			list_free.pop();
			LeaveCriticalSection(&g_free_cs);
		}
	}
}

/** Init le thread qui pour executer sur le core 2 **/
extern "C" int InitThread()
{
	return 0;
	InitializeCriticalSection( &g_CsQueue );
	InitializeCriticalSection( &g_free_cs );

	hThreadRun = CreateThread( 
        NULL,                   // default security attributes
        0,                      // use default stack size  
        ThreadRunner,			// thread function name
		NULL,					// argument to thread function 
        CREATE_SUSPENDED,       // use default creation flags 
        NULL
	);   // returns the thread identifier 

	XSetThreadProcessor(hThreadRun, 2);
	SetThreadPriority(hThreadRun ,THREAD_PRIORITY_HIGHEST);
	ResumeThread(hThreadRun);

/*
	hThreadFree = CreateThread( 
        NULL,                   // default security attributes
        0,                      // use default stack size  
        ThreadRunnerFree,		// thread function name
		NULL,					// argument to thread function 
        CREATE_SUSPENDED,       // use default creation flags 
        NULL
	);   // returns the thread identifier 

	XSetThreadProcessor(hThreadFree, 0);
	SetThreadPriority(hThreadFree ,THREAD_PRIORITY_LOWEST);
	ResumeThread(hThreadFree);
*/

	return 0;
}

extern "C" void AddFunc(void(*func) (void*),void * args){
	if(func == NULL)
		return;

	work_t cur;
	cur.func = func;
	cur.args = args;

	EnterCriticalSection( &g_CsQueue );

	qwork.push(cur);

	LeaveCriticalSection( &g_CsQueue );
}

//Appeler a chaque VSync
extern "C" void Core0FreeAll(){
	EnterCriticalSection( &g_free_cs );
	while (!list_free.empty())
	{
		void * addr = list_free.front();
		free(addr);
		list_free.pop();
	}
	LeaveCriticalSection( &g_free_cs );
}

extern "C" void Core0Free(void * addr){
	if(addr == NULL)
		return;

	EnterCriticalSection( &g_free_cs );

	list_free.push(addr);

	LeaveCriticalSection( &g_free_cs );
}