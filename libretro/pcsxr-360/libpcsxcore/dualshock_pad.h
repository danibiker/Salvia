/* DualShock state machine for PCSX-R 360.
 *
 * Portado de Pokopom (KrossX, GPLv3) — plugin de input PSX/PS2
 * incluido en 360/Xdk/pokopom_input/Pokopom/Controller.cpp.  Implementa
 * el SIO state machine completo del DualShock SCPH-1200:
 *   - Modo digital (ID 0x41) y analog (ID 0x73).
 *   - Config mode (ID 0xF3) con comandos 0x43/0x44/0x45/0x46/0x47/0x4C/0x4D.
 *   - Set rumble mapping (cmd 0x4D) y aplicacion de vibracion en cada poll.
 *
 * Este plugin reemplaza la version simplificada de _PADstartPoll/_PADpoll
 * que solo soportaba 0x42 (poll plano) y rompia juegos como Ape Escape o
 * Gran Turismo 2 que verifican el comportamiento real del DualShock antes
 * de aceptarlo.
 *
 * API usage:
 *
 *   1. ds_init(rumble_callback) — una sola vez al arranque del proceso.
 *   2. ds_reset(port) — al abrir el puerto / cambiar de juego.
 *   3. Antes de cada poll: ds_set_input(port, &input) con el state actual
 *      del usuario (buttons + sticks, ya en formato PSX).
 *   4. ds_command(port, counter, data) — uno por byte intercambiado.
 *      counter=0 para start (data=0x01), counter=1..N para polls.
 *
 * El state es per-port; los puertos son indices 0/1.
 */

#ifndef __DUALSHOCK_PAD_H__
#define __DUALSHOCK_PAD_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Snapshot del input del usuario para un poll.  Se rellena ANTES de
 * _PADstartPoll (que llama ds_command con counter=0).  El state machine
 * congela estos valores durante todo el intercambio del SIO; el siguiente
 * intercambio lee valores nuevos. */
typedef struct {
    /* Button bitfield en formato PSX (active low — 1 = no pulsado).
     * Layout estandar:
     *   bit 0: Select   bit 8: L2
     *   bit 1: L3       bit 9: R2
     *   bit 2: R3       bit 10: L1
     *   bit 3: Start    bit 11: R1
     *   bit 4: D-Up     bit 12: Triangle
     *   bit 5: D-Right  bit 13: Circle
     *   bit 6: D-Down   bit 14: Cross
     *   bit 7: D-Left   bit 15: Square
     */
    uint16_t buttons;

    /* Analog sticks en formato PSX (0..255, neutro=0x80, +Y=down). */
    uint8_t  leftX,  leftY;
    uint8_t  rightX, rightY;
} ds_input_t;

/* Callback que el state machine invoca cuando el juego envia rumble
 * values via 0x42.  port = 0/1 (puerto PSX), small = digital del DualShock
 * (0=off, !=0=on), big = analog 0..255. */
typedef void (*ds_rumble_cb_t)(int port, uint8_t small, uint8_t big);

/* Inicializa el state machine.  Pasa NULL si no quieres rumble.
 * Idempotente — safe llamar varias veces. */
void ds_init(ds_rumble_cb_t rumble_cb);

/* Resetea el state de un puerto a defaults.  Llamar al abrir el plugin
 * o cuando cambia el juego cargado. */
void ds_reset(int port);

/* Snapshot del input del usuario.  Llamar JUSTO antes del primer
 * ds_command(port, 0, ...) de un intercambio.  El state machine usa
 * estos valores durante todo el ciclo. */
void ds_set_input(int port, const ds_input_t *input);

/* Procesa un byte del intercambio SIO.  counter=0 para el start byte
 * (data=0x01); counter=1..8 para los polls subsecuentes (data = lo que
 * el juego escribe).  Retorna el byte que el pad responde. */
uint8_t ds_command(int port, uint32_t counter, uint8_t data);

/* Helper opcional: pone el pad en modo analog locked (ID = 0x73).  Util
 * cuando el frontend selecciona "DualShock" — fuerza analog en lugar de
 * dejar al juego/usuario togglear con el boton ANALOG.  PSXInput.cpp no
 * tiene boton ANALOG fisico que mapear, asi que el toggle del pad real
 * no aplica. */
void ds_force_analog(int port);

#ifdef __cplusplus
}
#endif

#endif /* __DUALSHOCK_PAD_H__ */
