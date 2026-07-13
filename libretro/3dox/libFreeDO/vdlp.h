/*
	3DOplay sources v1.7.3 based on FreeDOcore
	3doplay.do.am
	Developer: Viktor Ivanov
	Any uses of the 3DOplay sources or any other material published by Viktor Ivanov have to be accompanied with full credits.
All rights reserved.
*/
/*
  www.freedo.org
The first and only working 3DO multiplayer emulator.

The FreeDO licensed under modified GNU LGPL, with following notes:

*   The owners and original authors of the FreeDO have full right to develop closed source derivative work.
*   Any non-commercial uses of the FreeDO sources or any knowledge obtained by studying or reverse engineering
    of the sources, or any other material published by FreeDO have to be accompanied with full credits.
*   Any commercial uses of FreeDO sources or any knowledge obtained by studying or reverse engineering of the sources,
    or any other material published by FreeDO is strictly forbidden without owners approval.

The above notes are taking precedence over GNU LGPL in conflicting situations.

Project authors:

Alexander Troosh
Maxim Grishin
Allen Wright
John Sammons
Felix Lazarev
*/


#ifndef	VDLP_3DO_HEADER
#define VDLP_3DO_HEADER

#include "3doplay.h"

	void _vdl_Init(unsigned char *vramstart);
        void _vdl_ProcessVDL( unsigned int addr);

        void _vdl_DoLineNew(int line, struct VDLFrame *frame);

        unsigned int _vdl_SaveSize();
        void _vdl_Save(void *buff);
        void _vdl_Load(void *buff);

#endif
