/*
 * xbox/trucos_n64.h  --  los trucos GameShark de N64 (retro_cheat_set).
 *
 * Ver trucos_n64.c.
 */
#ifndef XBOX64_TRUCOS_N64_H
#define XBOX64_TRUCOS_N64_H

#ifdef __cplusplus
extern "C" {
#endif

/* retro_cheat_reset: quita todos los trucos (no restaura la memoria, como
 * mupen64plus-libretro). */
void xbox64_trucos_borrar(void);

/* retro_cheat_set: el truco `indice` con sus codigos en texto ("8033B21E 0008",
 * varios separados por '+' o espacios, como en los .cht de libretro). */
void xbox64_trucos_poner(unsigned indice, int activo, const char *texto);

/* Una vez por VI, desde new_vi (timers.c), en el hilo del emulador. */
void xbox64_trucos_vi(void);

/* Al cargar una ROM (InitTimer, timers.c): vuelve a contar los VI de arranque
 * y quita los trucos de la ROM anterior. */
void xbox64_trucos_rom_nueva(void);

#ifdef __cplusplus
}
#endif

#endif /* XBOX64_TRUCOS_N64_H */
