/*
 * xbox/input_n64.h  --  el plugin de mandos, hablando con libretro.
 *
 * La frontera C/C++ va por aqui, igual que plataforma.h: este fichero es C
 * y lo incluye el wrapper (C++) dentro de un `extern "C"`.
 *
 * C89.
 */

#ifndef XBOX64_INPUT_N64_H
#define XBOX64_INPUT_N64_H

/* --- lo que el wrapper llama --------------------------------------- */

/* Una vez por frame, JUSTO DESPUES de `input_poll_cb()`.  Toma la foto de
 * los cuatro mandos; `getKeys` no vuelve a preguntarle al frontend. */
void xbox64_input_poll(void);

/* `retro_set_controller_port_device`.  RETRO_DEVICE_NONE desenchufa el
 * mando de ese puerto; cualquier otra cosa lo enchufa. */
void xbox64_input_set_device(unsigned puerto, unsigned device);

/* Al cargar una ROM: la foto vuelve a cero para que el invitado no lea la
 * ultima pulsacion de la partida anterior. */
void xbox64_input_reset(void);

/* La zona muerta del stick izquierdo, en tanto por ciento del recorrido
 * (opcion xbox64_astick_deadzone, 0..30, 15 por defecto).  Radial y
 * reescalada, como mupen64plus-next.  Se aplica en la siguiente foto. */
void xbox64_input_set_zona_muerta(int por_ciento);

/* La curva de respuesta del stick izquierdo (opcion xbox64_astick_curva):
 * 0 lineal, 1 suave (t^1.5), 2 mas suave (t^2), 3 muy suave (t^3).  Se
 * aplica al recorrido que queda pasada la zona muerta; todas llegan al
 * maximo con el stick a fondo.  Se aplica en la siguiente foto. */
void xbox64_input_set_curva(int modo);

/* --- lo unico que el plugin necesita del wrapper -------------------- */

/* ★ LA DEFINE `libretro_core.cpp`, que es C++, y por eso la declaracion
 * vive aqui: la cabecera entra alli dentro de un `extern "C"`, asi que la
 * definicion hereda enlace C.  Es la misma frontera que en la fase 3 dejo
 * 27 simbolos sin resolver en tres oleadas; con la declaracion visible
 * antes de la definicion no hay forma de que se separen. */
int xbox64_retro_input_state(unsigned puerto, unsigned device,
                             unsigned indice, unsigned id);

#endif /* XBOX64_INPUT_N64_H */
