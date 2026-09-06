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

/* --- Canal POR RATON (no pasa por SDL) ------------------------------------
 * SDL 1.2 tiene un unico cursor global, asi que todo lo que se empuje por
 * SDL_PrivateMouseMotion se fusiona en uno solo. Para tener dos punteros
 * independientes (p.ej. dos RETRO_DEVICE_LIGHTGUN) el consumidor lee estos
 * deltas por dispositivo y se integra la posicion el mismo. Por SDL sigue yendo
 * solo el raton virtual agregado, que es lo que mueve el cursor del menu.
 *
 * Requiere un plugin con el bloque de extension; con uno antiguo DeviceCount
 * devuelve 0 y no hay nada por dispositivo (el agregado sigue funcionando). */

/* Numero de SLOTS de raton que expone el plugin, conectados o no. 0 = el plugin
 * no publica deltas por dispositivo. Los slots NO son contiguos: hay que
 * recorrerlos preguntando por DeviceConnected. */
int  XBOX_HIDMouse_DeviceCount(void);

/* 1 si ese slot tiene un raton conectado ahora mismo. */
int  XBOX_HIDMouse_DeviceConnected(int idx);

/* VID/PID del raton del slot (0 si no se pudo leer). Los slots se asignan por
 * primer hueco libre, asi que un replug puede reordenarlos: esto es lo que
 * permite fijar "este raton es el jugador 2" si hace falta. */
int  XBOX_HIDMouse_DeviceInfo(int idx, unsigned int* vid, unsigned int* pid);

/* Deltas acumulados de UN slot desde la llamada anterior (misma convencion que
 * XBOX_HIDMouse_Drain, con su propio "ultimo leido" por slot). */
void XBOX_HIDMouse_DrainDevice(int idx, int* dx, int* dy, int* dwheel,
                               unsigned int* buttons);

/** Indica si se ha podido cargar el plugin del raton */
int XBOX_isHidMousePluginConnected();


#ifdef __cplusplus
}
#endif

#endif /* SDL_XBOXHIDMOUSE_H */
