//
// Copyright (c) 2004 K. Wilkins
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//

//////////////////////////////////////////////////////////////////////////////
//                       Handy - An Atari Lynx Emulator                     //
//                          Copyright (c) 1996,1997                         //
//                                 K. Wilkins                               //
//////////////////////////////////////////////////////////////////////////////
// Lynx memory map object header file                                       //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This header file provides the interface definition for the memory map    //
// object, this object controls which pieces of lynx hardware are           //
// accesible by the CPU at any given time, it is the code for addr $FFF9    //
//                                                                          //
//    K. Wilkins                                                            //
// August 1997                                                              //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
// Revision History:                                                        //
// -----------------                                                        //
//                                                                          //
// 01Aug1997 KW Document header added & class documented.                   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#ifndef MEMMAP_H
#define MEMMAP_H

#define MEMMAP_SIZE				0x1

class CMemMap : public CLynxBase
{
	// Function members

	public:
		CMemMap(CSystem& parent) MDFN_COLD;

	public:
		void	Reset(void) MDFN_COLD;

		void	Poke(uint32 addr,uint8 data);
		uint8	Peek(uint32 addr);
		uint32	ReadCycle(void) {return 5;};
		uint32	WriteCycle(void) {return 5;};
		uint32	ObjectSize(void) {return MEMMAP_SIZE;};
		int	StateAction(StateMem *sm, int load, int data_only);

	// Data members

	private:
		int				mMikieEnabled;
		int				mSusieEnabled;
		int				mRomEnabled;
		int				mVectorsEnabled;

		CSystem&		mSystem;
};

#endif

