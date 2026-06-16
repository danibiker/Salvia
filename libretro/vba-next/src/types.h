/* VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator. */
/* Copyright (C) 2008 VBA-M development team */

/* This program is free software; you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation; either version 2, or(at your option) */
/* any later version. */
/* */
/* This program is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */
/* */
/* You should have received a copy of the GNU General Public License */
/* along with this program; if not, write to the Free Software Foundation, */
/* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#ifndef __VBA_TYPES_H__
#define __VBA_TYPES_H__

#include <stdint.h>

/* bool/true/false: under C89 (no <stdbool.h>) and pre-VS2013 MSVC, the
 * libretro shim defines them as `unsigned char` / 1 / 0; under C99+ it just
 * pulls in <stdbool.h>; under C++ they're builtin (header self-disables). */
#include <boolean.h>

#endif /* __VBA_TYPES_H__ */
