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
// RAM emulation class                                                      //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This class emulates the system RAM (64KB), the interface is pretty       //
// simple: constructor, reset, peek, poke.                                  //
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


#define RAM_CPP

#include "system.h"
#include "ram.h"
#include "../mempatcher.h"
#include "../mednafen-endian.h"
#include <../md5.h>
#include "../../scrc32.h"
#include <algorithm>

CRam::CRam(MDFNFILE *fp)
	:mRamXORData(NULL)
{
	if(fp)
	{
		uint8 raw_header[HEADER_RAW_SIZE];
		md5_context md5;
		md5.starts();
		mCRC32 = 0;

		file_read(fp, raw_header, sizeof(raw_header), 1);
      file_seek(fp, 0, SEEK_SET);

		if(memcmp(&raw_header[6], "BS93", 4))
		{
		 /* Lynx file format invalid (Magic No) */
		}

		mRamXORData = new uint8[RAM_SIZE];
		memset(&mRamXORData[0], 0, RAM_SIZE);

		const uint16   load_address = MDFN_de16msb(&raw_header[2]) - sizeof(raw_header);
		const uint16   size = MDFN_de16msb(&raw_header[4]);
		const unsigned rc0 = std::min<unsigned>((RAM_SIZE - load_address), size);
		const unsigned rc1 = size - rc0;

		file_read(fp, &mRamXORData[load_address], rc0, 1);
		md5.update(&mRamXORData[load_address], rc0);
		mCRC32 = crc32(mCRC32, &mRamXORData[load_address], rc0);
		file_read(fp, &mRamXORData[0x0000], rc1, 1);
		md5.update(&mRamXORData[0x0000], rc1);
		mCRC32 = crc32(mCRC32, &mRamXORData[0x0000], rc1);

		InfoRAMSize = size;

		for(unsigned i = 0; i < RAM_SIZE; i++)
		 mRamXORData[i] ^= DEFAULT_RAM_CONTENTS;

		boot_addr = load_address;
	}
	else
	 InfoRAMSize = 0;

	// Reset will cause the loadup
	Reset();
}

CRam::~CRam()
{
	if (mRamXORData != NULL)
		delete[] mRamXORData;
}

void CRam::Reset(void)
{
	MDFNMP_AddRAM(65536, 0x0000, mRamData);

	for(unsigned i = 0; i < RAM_SIZE; i++)
	 mRamData[i] = DEFAULT_RAM_CONTENTS;

	if(mRamXORData)
	{
	 for(unsigned i = 0; i < RAM_SIZE; i++)
	  mRamData[i] ^= mRamXORData[i];

	 gCPUBootAddress = boot_addr;
	}
}

//END OF FILE
