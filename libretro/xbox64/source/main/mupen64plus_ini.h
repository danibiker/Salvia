/**
 * xbox64 - mupen64plus_ini.h
 *
 * [Xbox360] [BattleTanx, 2026-10-30] La CountPerOp de cada juego, leida de
 * mupen64plus.ini (la base de datos de mupen64plus-core), como la lee
 * mupen64plus: por el MD5 de la ROM y, si no esta, por el CRC de la
 * cabecera; una entrada con RefMD5 hereda el valor de la referenciada.
 *
 * El fichero se busca en el directorio system del frontend:
 *   system\mupen64plus.ini
 *   system\Mupen64plus\mupen64plus.ini   (donde lo deja mupen64plus-libretro-nx)
 *
 * Ver mupen64plus_ini.c.
**/

#ifndef MUPEN64PLUS_INI_H
#define MUPEN64PLUS_INI_H

#ifdef __cplusplus
extern "C" {
#endif

/* La CountPerOp que pone mupen64plus.ini para la ROM cargada (1..4), o 0 si
 * no hay fichero, el juego no esta o su entrada no la trae (mupen64plus usa
 * entonces 2, su valor por defecto, que es tambien el de este core).
 * `crc1`/`crc2`: los de la cabecera (0x10 y 0x14), para cuando el MD5 no
 * esta.  Llamar con la ROM ya en la cache (tras ROMCache_load). */
int mupen64plus_ini_count_per_op(unsigned int crc1, unsigned int crc2);

#ifdef __cplusplus
}
#endif

#endif /* MUPEN64PLUS_INI_H */
