/***************************************************************************
 *   Copyright (C) 2007 Ryan Schultz, PCSX-df Team, PCSX team              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
 ***************************************************************************/

#ifndef __DECODE_XA_H__
#define __DECODE_XA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "psxcommon.h"

typedef struct {
	s32	y0, y1;
} ADPCM_Decode_t;

/* Named `struct xa_decode` (in addition to the typedef) so that
 * forward declarations like `struct xa_decode;` in dfsound/externals.h
 * resolve to the same type as `xa_decode_t`.  MSVC treats anonymous-
 * tagged typedefs and `struct foo;` forwards as distinct types, so
 * giving the struct a tag is what lets the SPU plugin and the cdrom
 * code share the type without an explicit decode_xa.h include. */
typedef struct xa_decode {
	int				freq;
	int				nbits;
	int				stereo;
	int				nsamples;
	ADPCM_Decode_t	left, right;
	short			pcm[16384];
} xa_decode_t;

s32 xa_decode_sector( xa_decode_t *xdp,
					   unsigned char *sectorp,
					   int is_first_sector );

#ifdef __cplusplus
}
#endif
#endif
