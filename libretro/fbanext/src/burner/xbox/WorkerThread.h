#pragma once

#include <process.h>
#include "cthread.h"


class WorkerThread : public CThread
{
public:

        WorkerThread() {}
        ~WorkerThread( void ) {}
        bool CreateWorkerThread( void );

protected:
        virtual unsigned long Process (void* parameter);        
};