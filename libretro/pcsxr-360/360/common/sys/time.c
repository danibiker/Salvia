#define EPOCHFILETIME (116444736000000000i64)

#include <xtl.h>
#include "time.h"

void usleep(int x)
{
	Sleep(x/1000);
};

int fork(){
	return -1;
}


int gettimeofday(struct timeval *tv, struct timezone *tz){
	FILETIME        ft;
	LARGE_INTEGER   li;
	__int64         t;
	static int      tzflag;

	if (tv)
	{
		GetSystemTimeAsFileTime(&ft);
		li.LowPart  = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		t  = li.QuadPart;       /* In 100-nanosecond intervals */
		t -= EPOCHFILETIME;     /* Offset to the Epoch time */
		t /= 10;                /* In microseconds */
		tv->tv_sec  = (long)(t / 1000000);
		tv->tv_usec = (long)(t % 1000000);
	}

	return 0;
}