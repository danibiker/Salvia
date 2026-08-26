/*
 * UAE - The Un*x Amiga Emulator
 *
 * Copyright 2004 Richard Drummond
 *
 * Start-up and support functions used by Linux/Unix targets
 */

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "debug.h"

#undef __unix

/* WinUAE define estos tres en od-win32/win32.cpp, que es su capa de target y
 * aqui no se compila; custom.c los declara extern bajo #ifdef DEBUGGER y los
 * usa para instrumentar el temporizado de vsync. A 0 el comportamiento es el
 * de por defecto: sin traza y sin retardo forzado ni minimo. */
int log_vsync = 0;
int debug_vsync_min_delay = 0;
int debug_vsync_forced_delay = 0;

/*
 * Handle break signal
 */
#ifdef HAVE_SIGNAL
#include <signal.h>
#endif

#ifdef __cplusplus
static RETSIGTYPE sigbrkhandler(...)
#else
static RETSIGTYPE sigbrkhandler (int foo)
#endif
{
}

void setup_brkhandler (void)
{
#if defined(__unix) && !defined(__NeXT__)
    struct sigaction sa;
    sa.sa_handler = sigbrkhandler;
    sa.sa_flags = 0;
#ifdef SA_RESTART
    sa.sa_flags = SA_RESTART;
#endif
    sigemptyset (&sa.sa_mask);
    sigaction (SIGINT, &sa, NULL);
#elif defined(HAVE_SIGNAL)
    signal (SIGINT, sigbrkhandler);
#endif
}
