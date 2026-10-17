#include <io/presentclock.h>

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif

#include <SDL.h>

/* El backend de video expone los contadores del hardware; en Windows devuelven
 * ceros porque D3D9 de escritorio no tiene callback de vblank.  Solo por WIN,
 * como engine.h: SALVIA_GPU_VIDEO vive en const/constant.h, que aqui no se
 * incluye, y con el la condicion salia siempre falsa (C3861 en Windows). */
#ifdef WIN
#include <video/win_d3d9.h>
#endif

/* ==========================================================================
 * Implementacion.  Ver presentclock.h para el porque.
 *
 * Todo lo especifico de plataforma esta detras de las funciones del backend de
 * video (`SDL_XBOX_*`), que ya existian o se anadieron con los contadores.  Aqui
 * no hay nada de D3D: esto es politica, no driver.
 * ========================================================================== */

unsigned int pc_vblanks(void)
{
    unsigned int n = 0, miss = 0;
    SDL_XBOX_GetVBlankStats(&n, &miss);
    return n;
}

unsigned int pc_swaps_missed(void)
{
    unsigned int n = 0, miss = 0;
    SDL_XBOX_GetVBlankStats(&n, &miss);
    return miss;
}

int pc_phase_percent(void)
{
    return SDL_XBOX_GetScanlinePercent();
}

float pc_refresh_hz(void)
{
    return SDL_XBOX_GetRefreshRate();
}

unsigned int pc_wait_vblank(unsigned int objetivo, unsigned int msMax)
{
    unsigned int n = pc_vblanks();

    if (n == 0)
        return 0;               /* sin reloj de hardware */

    /* Comparacion CON SIGNO sobre la resta, no `n < objetivo`: el contador es
     * de 32 bits y al girar -cada ~2,3 anos a 60 Hz, pero tambien si el driver
     * lo reinicia en un cambio de modo- una comparacion sin signo daria una
     * espera de 2^32 vblanks.  Es el mismo error que cometimos en el nucleo con
     * `next_interupt <= Count`. */
    while ((int)(objetivo - n) > 0)
    {
        /* Espera BLOQUEANTE, que es la mitad del valor de todo esto: el hilo se
         * duerme y el core se queda con la CPU.  El tope corto hace dos cosas:
         * acota el dano de un pulso perdido -el bucle vuelve a comprobar el
         * contador, que es la verdad- y garantiza que un cambio de modo de video
         * no cuelgue el frontend. */
        if (!SDL_XBOX_WaitVBlank(2))
            break;              /* el backend no sabe esperar: que decida el llamante */

        n = pc_vblanks();

        /* `msMax` en milisegundos, y cada vuelta consume 2 como maximo. */
        if (msMax <= 2)
            break;
        msMax -= 2;
    }

    return n;
}
