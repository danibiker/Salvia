/*
    SDL - Simple DirectMedia Layer / Xbox 360 native USB HID mouse

    Lector de raton USB HID nativo para Xbox 360 (consolas CFW: RGH/JTAG).
    Portado de la ruta boot-protocol del plugin DashLaunch x360remap
    (hiddriver, fork de hiddriver360 de EinTim23).

    A diferencia del plugin original -que es un sysdll residente que convierte
    el raton en un mando XInput virtual a nivel de sistema- aqui se hookea la
    pila USB HID del kernel DESDE el propio .xex de Salvia y se entregan los
    deltas crudos a SDL via SDL_PrivateMouseMotion/Button.

    Cabecera deliberadamente autocontenida (sin dependencias de SDL ni D3D) para
    poder incluirse desde ficheros C y C++, y desde builds Xbox y Win32.
*/

#ifndef SDL_XBOXHIDMOUSE_H
#define SDL_XBOXHIDMOUSE_H

/* --- Toggle maestro -------------------------------------------------------
 * Comenta la siguiente linea para desactivar por completo el raton USB HID
 * nativo (el codigo de parcheo de kernel no se compilara ni se instalara, y
 * mouse_update() volvera a su comportamiento anterior).
 *
 * Solo tiene efecto en builds de Xbox 360 (_XBOX). Requiere una consola con
 * CFW (RGH/JTAG): parchea codigo del kernel, algo que solo funciona con las
 * protecciones de pagina desactivadas (freeboot). En retail sin CFW la
 * inicializacion falla de forma segura (fail-safe) y el raton queda inactivo.
 */
#ifdef _XBOX
#define SDL_XBOX_HIDMOUSE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Instala los hooks de kernel (HidAddDevice/HidRemoveDevice) y resuelve las
 * funciones del stack USB. Idempotente. Devuelve 1 si el raton HID queda
 * activo, 0 si no se pudo (build de dashboard no soportado, no CFW, etc.). */
int  XBOX_HIDMouse_Init(void);

/* Retira los hooks y libera los recursos de los ratones aun conectados.
 * OBLIGATORIO antes de que el .xex se descargue (si no, el kernel saltaria a
 * memoria liberada). Idempotente. */
void XBOX_HIDMouse_Quit(void);

/* Lee y pone a cero los acumuladores de movimiento/rueda; 'buttons' es el
 * estado de nivel actual de los botones (bit0=izq, bit1=der, bit2=medio, por
 * convencion del boot-mouse HID). Llamar una vez por poll de SDL. */
void XBOX_HIDMouse_Drain(int* dx, int* dy, int* dwheel, unsigned int* buttons);

/* Drena el raton HID y despacha los eventos SDL correspondientes. Definida en
 * SDL_xboxmouse.c; llamada desde mouse_update() en SDL_xboxevents.c. */
void XBOX_MouseUpdateHID(void);

#ifdef __cplusplus
}
#endif

#endif /* SDL_XBOXHIDMOUSE_H */
