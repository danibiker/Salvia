/**
 * Wii64 - gamehacks.h
 * Copyright (C) 2012 emu_kidid
 * 
 * Game specific hacks to workaround core inaccuracy
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: emukidid@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#ifndef GAMEHACKS_H_
#define GAMEHACKS_H_

/* [Xbox360] ★ ENLACE C, QUE EL ORIGINAL NO DECLARA.
 *
 * `main/gamehacks.c` es C, y estas ocho funciones las llaman CUATRO
 * ficheros C++ del plugin grafico, que incluyen esta cabecera:
 *
 *     glN64_GX/gDP.cpp:1552      == &hack_yoshistory
 *     glN64_GX/gSP.cpp:3002      == &hack_ogrebattle
 *     glN64_GX/OpenGL.cpp:836    == &hack_scars
 *     glN64_GX/OpenGL.cpp:1955   == &hack_dukenukem
 *     glN64_GX/RDP.cpp:299       == &hack_winback
 *
 * Sin `extern "C"`, esos cinco sitios buscan el simbolo DECORADO y el .c
 * define el simbolo pelado: cinco LNK2019.
 *
 * Wii64 compila con devkitPPC y no lo lleva; el arbol del que viene el
 * shim SI se lo anyadio a su copia, y su build enlaza.  Se hace igual. */
#ifdef __cplusplus
extern "C" {
#endif

// Return a pointer to the game specific hack or 0 if there isn't any
void *GetGameSpecificHack();

/* [Xbox360] Enciende o apaga los parches por juego.  Por defecto encendidos;
 * ver el comentario de `xbox64_gamehacks_habilitar` en gamehacks.c. */
void xbox64_gamehacks_habilitar(int on);

// Game specific hack detection via CRC
void GameSpecificHackSetup();

void hack_winback();
void hack_paperboy();
void hack_dukenukem();
void hack_scars();
void hack_ogrebattle();
void hack_yoshistory();

#ifdef __cplusplus
}
#endif

#endif
