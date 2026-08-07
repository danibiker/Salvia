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
// Lynx Cartridge Class                                                     //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This class emulates the Lynx cartridge interface, given a filename it    //
// will contstruct a cartridge object via the constructor.                  //
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

#define CART_CPP

#include "system.h"

#include <algorithm>
#include <string.h>
#include "cart.h"
#include "../state.h"
#include "../md5.h"
#include "../../scrc32.h"
#include "../mednafen-endian.h"

static LYNX_DB lynxDB[] = {
   { 0x540e9bb7, "Alien vs Predator (USA) (Proto) (1993-12-17)", 262144, 0, 0 },
   { 0xf6fb48fb, "A.P.B. (USA, Europe)", 262144, 0, 0 },
   { 0x0483cd2a, "Awesome Golf (USA, Europe)", 262144, 0, 0 },
   { 0x3943c116, "Baseball Heroes (USA, Europe)", 262144, 0, 0 },
   { 0x4161bb4a, "Basketbrawl (USA, Europe)", 131072, 0, 0 },
   { 0x277f82c2, "Batman Returns (USA, Europe)", 262144, 0, 0 },
   { 0x779faece, "Battle Wheels (USA, Europe)", 131072, 0, 0 },
   { 0x30fee726, "Battlezone 2000 (USA, Europe)", 262144, 0, 0 },
   { 0x143a313e, "Bill & Ted's Excellent Adventure (USA, Europe)", 262144, 0, 0 },
   { 0x0d973c9d, "[BIOS] Atari Lynx (USA, Europe)", 512, 0, 0 },
   { 0x3cd75df3, "Block Out (USA, Europe)", 131072, 0, 0 },
   { 0xdaf587b1, "Blue Lightning (USA, Europe)", 131072, 0, 0 },
   { 0xbfe36525, "Blue Lightning (USA, Europe) (Demo)", 131072, 0, 0 },
   { 0x333daece, "Bubble Trouble (USA, Europe)", 262144, 0, 0 },
   { 0xa08f0b59, "California Games (USA, Europe)", 131072, 0, 0 },
   { 0x97501709, "Centipede (USA) (Proto)", 131072, CART_ROTATE_LEFT, 0 },
   { 0x19c5a7a5, "Checkered Flag (USA, Europe)", 262144, 0, 0 },
   { 0x6a5f53ed, "Chip's Challenge (USA, Europe)", 131072, 0, 0 },
   { 0xaec474c8, "Crystal Mines II (USA, Europe)", 131072, 0, 0 },
   { 0x99729395, "Daemonsgate (USA) (Proto)", 262144, 0, 0 },
   { 0xb9ac1fe5, "Desert Strike - Return to the Gulf (USA, Europe)", 262144, 0, 0 },
   { 0x50386cfa, "Dinolympics (USA, Europe)", 262144, 0, 0 },
   { 0xd565fbb7, "Dirty Larry - Renegade Cop (USA, Europe)", 262144, 0, 0 },
   { 0xfbfc0f05, "Double Dragon (USA, Europe)", 262144, 0, 0 },
   { 0x33bb74c7, "Dracula the Undead (USA, Europe)", 262144, 0, 0 },
   { 0xbd97116b, "Electrocop (USA, Europe)", 131072, 0, 0 },
   { 0xf83397f9, "European Soccer Challenge (USA, Europe)", 131072, 0, 0 },
   { 0x6bceaa9c, "Eye of the Beholder (USA) (Proto)", 131072, 0, 0 },
   { 0x9034ee27, "Fat Bobby (USA, Europe)", 262144, 0, 0 },
   { 0x7e4b5945, "Fidelity Ultimate Chess Challenge, The (USA, Europe)", 131072, 0, 0 },
   { 0x494cc568, "Gates of Zendocon (USA, Europe)", 131072, 0, 0 },
   { 0xac564baa, "Gauntlet - The Third Encounter (1990) [o1]", 262144, CART_ROTATE_RIGHT, 0 },
   { 0x7f0ec7ad, "Gauntlet - The Third Encounter (USA, Europe)", 131072, CART_ROTATE_RIGHT, 0 },
   { 0xd20a85fc, "Gordo 106 (USA, Europe)", 262144, 0, 0 },
   { 0x6df63834, "Hard Drivin' (USA, Europe)", 131072, 0, 0 },
   { 0xe8b45707, "Hockey (USA, Europe)", 262144, 0, 0 },
   { 0xe3041c6c, "Hydra (USA, Europe)", 262144, 0, 0 },
   { 0x5cf8bbf0, "Ishido - The Way of Stones (USA, Europe)", 131072, 0, 0 },
   { 0x2455b6cf, "Jimmy Connors' Tennis (USA, Europe)", 524288, 0, 0 },
   { 0x5dba792a, "Joust (USA, Europe)", 131072, 0, 0 },
   { 0xa53649f1, "Klax (USA, Europe)", 262144, CART_ROTATE_RIGHT, 0 },
   { 0xbed5ba2b, "Krazy Ace - Miniature Golf (USA, Europe)", 262144, 0, 0 },
   { 0xcd1bd405, "Kung Food (USA, Europe)", 262144, 0, 0 },
   { 0x39b9b8cc, "Lemmings (USA, Europe)", 262144, 0, 0 },
   { 0x0271b6e9, "Lexis (USA)", 262144, CART_ROTATE_LEFT, 0 },
   { 0xb1c25ef1, "Loopz (USA) (Proto)", 262144, 0, 0 },
   { 0x1091a268, "Lynx Casino (USA, Europe)", 262144, 0, 0 },
   { 0x28ada019, "Lynx II Production Test Program (USA) (v0.02) (Proto)", 262144, 0, 0 },
   { 0xaba6da3d, "Malibu Bikini Volleyball (USA, Europe)", 262144, 0, 0 },
   { 0xc3fa0d4d, "Marlboro Go! (Europe) (Proto)", 262144, 0, 0 },
   { 0x7de3783a, "Ms. Pac-Man (USA, Europe)", 131072, 0, 0 },
   { 0x006fd398, "NFL Football (USA, Europe)", 262144, CART_ROTATE_LEFT, 0 },
   { 0xf3e3f811, "Ninja Gaiden III - The Ancient Ship of Doom (USA, Europe)", 524288, 0, 0 },
   { 0x22d47d51, "Ninja Gaiden (USA, Europe)", 262144, 0, 0 },
   { 0xaa50dd22, "Pac-Land (USA, Europe)", 131072, 0, 0 },
   { 0x4cdfbd57, "Paperboy (USA, Europe)", 131072, 0, 0 },
   { 0x14d38ca7, "Pinball Jam (USA, Europe)", 262144, 0, 0 },
   { 0x2393135f, "Pit-Fighter (USA, Europe)", 524288, 0, 0 },
   { 0x99c42034, "Power Factor (USA, Europe)", 262144, 0, 0 },
   { 0xb9881423, "QIX (USA, Europe)", 131072, 0, 0 },
   { 0xbcd10c3a, "Raiden (USA) (v3.0) (Beta)", 262144, CART_ROTATE_LEFT, 0 },
   { 0xb10b7c8e, "Rampage (USA, Europe)", 262144, 0, 0 },
   { 0x139f301d, "Rampart (USA, Europe)", 262144, 0, 0 },
   { 0x6867e80c, "RoadBlasters (USA, Europe)", 262144, 0, 0 },
   { 0x69959a3b, "Road Riot 4WD (USA) (Proto 3)", 262144, 0, 0 },
   { 0xd1dff2b2, "Robo-Squash (USA, Europe)", 131072, 0, 0 },
   { 0x7a6049b5, "Robotron 2084 (USA, Europe)", 131072, 0, 0 },
   { 0x67e5bdba, "Rygar (USA, Europe)", 262144, 0, 0 },
   { 0xbe166f3b, "Scrapyard Dog (USA, Europe)", 262144, 0, 0 },
   { 0xeb78baa3, "Shadow of the Beast (USA, Europe)", 262144, 0, 0 },
   { 0x192bcd04, "Shanghai (USA, Europe)", 131072, 0, 0 },
   { 0x5b2308ed, "Steel Talons (USA, Europe)", 262144, 0, 0 },
   { 0x8595c40b, "S.T.U.N. Runner (USA, Europe)", 262144, 0, 0 },
   { 0x2da7e2a8, "Super Asteroids, Missile Command (USA, Europe)", 131072, 0, 0 },
   { 0x690caeb0, "Super Off-Road (USA, Europe)", 262144, 0, 0 },
   { 0xdfa61571, "Super Skweek (USA, Europe)", 262144, 0, 0 },
   { 0x13657705, "Switchblade II (USA, Europe)", 262144, 0, 0 },
   { 0xae267e29, "Todd's Adventures in Slime World (USA, Europe)", 131072, 0, 0 },
   { 0x156a4a4c, "Toki (USA, Europe)", 262144, 0, 0 },
   { 0x0590a9e3, "Tournament Cyberball (USA, Europe)", 262144, 0, 0 },
   { 0xa4b924d6, "Turbo Sub (USA, Europe)", 131072, 0, 0 },
   { 0x8d56828b, "Viking Child (USA, Europe)", 262144, 0, 0 },
   { 0xb946ba49, "Warbirds (USA, Europe)", 131072, 0, 0 },
   { 0x91233794, "World Class Soccer (USA, Europe)", 262144, 0, 0 },
   { 0x9bed736d, "Xenophobe (USA, Europe)", 131072, 0, 0 },
   { 0x89e2a595, "Xybots (USA, Europe)", 262144, 0, 0 },
   { 0xcb27199d, "Zarlor Mercenary (USA, Europe)", 131072, 0, 0 },

   { 0, NULL, 0, 0, 0 },
};

LYNX_DB CCart::CheckHash(const uint32 crc32)
{
	LYNX_DB ret = {};
	unsigned i = 0;

	found = false;
	while (lynxDB[i].crc32 != 0)
	{
		if (lynxDB[i].crc32 == crc32)
		{
			found = true;
			ret = lynxDB[i];
			break;
		}
		i++;
	}
	return ret;
}

LYNX_HEADER CCart::DecodeHeader(const uint8 *data)
{
 LYNX_HEADER header;

 memcpy(header.magic, data, 4);
 data += 4;

 header.page_size_bank0 = MDFN_de16lsb(data);
 data += 2;

 header.page_size_bank1 = MDFN_de16lsb(data);
 data += 2;

 header.version = MDFN_de16lsb(data);
 data += 2;

 memcpy(header.cartname, data, 32);
 data += 32;

 memcpy(header.manufname, data, 16);
 data += 16;

 header.rotation = *data;
 data++;

 memcpy(header.spare, data, 5);
 data += 5;

 return(header);
}

CCart::CCart(MDFNFILE *fp)
{
	uint64 gamesize;
	uint8 raw_header[HEADER_RAW_SIZE];
	LYNX_HEADER	header;
	uint32 header_size = HEADER_RAW_SIZE;
	uint32 loop;

	mWriteEnableBank0=false;
	mWriteEnableBank1=false;
	mCartRAM=false;
	mCRC32=0;

	if(fp)
	{
		gamesize = fp->size;

		// Calculate file's CRC32
		mCRC32 = crc32(0, fp->data, gamesize);

		// Checkout the header bytes
		file_read(fp, raw_header, sizeof(LYNX_HEADER), 1);
		header = DecodeHeader(raw_header);

		// Sanity checks on the header
		if(header.magic[0]!= 'L' || header.magic[1]!='Y' || header.magic[2]!='N' || header.magic[3]!='X' || header.version!=1)
		{
			header_size = 0;
			file_seek(fp, 0, SEEK_SET);
			memset(&header, 0, HEADER_RAW_SIZE);
			strncpy((char*)&header.cartname, "NO HEADER", 32);
			strncpy((char*)&header.manufname, "HANDY", 16);
			header.page_size_bank0 = gamesize >> 8; // Hard workaround...
		}
	  	else
			gamesize -= HEADER_RAW_SIZE;
	}
	else
	{
		header.page_size_bank0=0x000;
		header.page_size_bank1=0x000;

		strncpy((char*)&header.cartname, "NO HEADER", 32);
		strncpy((char*)&header.manufname, "HANDY", 16);

		// Setup rotation
		header.rotation = 0;

		gamesize = HEADER_RAW_SIZE;
	}

	InfoROMSize = gamesize;
	mCRC32 = 0;

    if (fp)
    {
       // re-calculate file crc32 minus header if any
       mCRC32     = crc32(0, fp->data + header_size, gamesize);
       LYNX_DB db = CheckHash(mCRC32);
       if (found)
       {
          header.page_size_bank0 = db.filesize >> 8;
          header.rotation        = db.rotation;
       }
    }

    // Setup name & manufacturer
    strncpy(mName, (char *)&header.cartname, 32);
    strncpy(mManufacturer, (char *)&header.manufname, 16);

	// Setup rotation
	mRotation=header.rotation;
	if(mRotation!=CART_NO_ROTATE && mRotation!=CART_ROTATE_LEFT && mRotation!=CART_ROTATE_RIGHT) mRotation=CART_NO_ROTATE;

	// Set the filetypes

	CTYPE banktype1;

    switch(header.page_size_bank0)
   {
      case 0x000:
         mMaskBank0=0;
         mShiftCount0=0;
         mCountMask0=0;
         break;
      case 0x100:
         mMaskBank0=0x00ffff;
         mShiftCount0=8;
         mCountMask0=0x0ff;
         break;
      case 0x200:
         mMaskBank0=0x01ffff;
         mShiftCount0=9;
         mCountMask0=0x1ff;
         break;
      case 0x400:
         mMaskBank0=0x03ffff;
         mShiftCount0=10;
         mCountMask0=0x3ff;
         break;
      case 0x800:
         mMaskBank0=0x07ffff;
         mShiftCount0=11;
         mCountMask0=0x7ff;
         break;
      default:
         break;
   }

	switch(header.page_size_bank1)
	{
		case 0x000:
			banktype1=UNUSED;
			mMaskBank1=0;
			mShiftCount1=0;
			mCountMask1=0;
			break;
		case 0x100:
			banktype1=C64K;
			mMaskBank1=0x00ffff;
			mShiftCount1=8;
			mCountMask1=0x0ff;
			break;
		case 0x200:
			banktype1=C128K;
			mMaskBank1=0x01ffff;
			mShiftCount1=9;
			mCountMask1=0x1ff;
			break;
		case 0x400:
			banktype1=C256K;
			mMaskBank1=0x03ffff;
			mShiftCount1=10;
			mCountMask1=0x3ff;
			break;
		case 0x800:
			banktype1=C512K;
			mMaskBank1=0x07ffff;
			mShiftCount1=11;
			mCountMask1=0x7ff;
			break;
		default:
			break;
	}

    // Make some space for the new carts

	mCartBank0 = new uint8[mMaskBank0+1];
	mCartBank1 = new uint8[mMaskBank1+1];

	// Set default bank

	mBank=bank0;

	// Initialiase

	for(loop=0;loop<mMaskBank0+1;loop++)
		mCartBank0[loop] = DEFAULT_CART_CONTENTS;

	for(loop=0;loop<mMaskBank1+1;loop++)
		mCartBank1[loop] = DEFAULT_CART_CONTENTS;

	// Read in the BANK0 bytes

	md5_context md5;
	md5.starts();

	if (mMaskBank0)
   {
		uint64 size = std::min<uint64>(gamesize, mMaskBank0 + 1);
		file_read(fp, mCartBank0, size, 1);
		md5.update(mCartBank0, size);
		gamesize -= size;
	}

	// Read in the BANK1 bytes
	if (mMaskBank1)
   {
		uint64 size = std::min<uint64>(gamesize, mMaskBank0 + 1);
		file_read(fp, mCartBank1, size, 1);
		md5.update(mCartBank1, size);
	}

	// As this is a cartridge boot unset the boot address

	gCPUBootAddress=0;

	// Dont allow an empty Bank1 - Use it for shadow SRAM/EEPROM
	if(banktype1==UNUSED)
	{
		// Delete the single byte allocated  earlier
		delete[] mCartBank1;
		// Allocate some new memory for us
		banktype1=C64K;
		mMaskBank1=0x00ffff;
		mShiftCount1=8;
		mCountMask1=0x0ff;
		mCartBank1 = new uint8[mMaskBank1+1];
		for(loop=0;loop<mMaskBank1+1;loop++) mCartBank1[loop]=DEFAULT_RAM_CONTENTS;
		mWriteEnableBank1=true;
		mCartRAM=true;
	}
}

CCart::~CCart()
{
	delete[] mCartBank0;
	delete[] mCartBank1;
}


void CCart::Reset(void)
{
	mCounter=0;
	mShifter=0;
	mAddrData=0;
	mStrobe=0;
	last_strobe = 0;
}

INLINE void CCart::Poke(uint32 addr, uint8 data)
{
	if(mBank==bank0)
	{
		if(mWriteEnableBank0) mCartBank0[addr&mMaskBank0]=data;
	}
	else
	{
		if(mWriteEnableBank1) mCartBank1[addr&mMaskBank1]=data;
	}
}


INLINE uint8 CCart::Peek(uint32 addr)
{
	if(mBank==bank0)
		return(mCartBank0[addr&mMaskBank0]);
	return(mCartBank1[addr&mMaskBank1]);
}


void CCart::CartAddressStrobe(bool strobe)
{
	mStrobe=strobe;

	if(mStrobe) mCounter=0;

	// Either of the two below seem to work OK.
	if(mStrobe && !last_strobe)
	{
		// Clock a bit into the shifter
		mShifter=mShifter<<1;
		mShifter+=mAddrData?1:0;
		mShifter&=0xff;
	}
	last_strobe=mStrobe;
}

void CCart::CartAddressData(bool data)
{
	mAddrData=data;
}


void CCart::Poke0(uint8 data)
{
	if(mWriteEnableBank0)
	{
		uint32 address=(mShifter<<mShiftCount0)+(mCounter&mCountMask0);
		mCartBank0[address&mMaskBank0]=data;
	}
	if(!mStrobe)
	{
		mCounter++;
		mCounter&=0x07ff;
	}
}

void CCart::Poke1(uint8 data)
{
	if(mWriteEnableBank1)
	{
		uint32 address=(mShifter<<mShiftCount1)+(mCounter&mCountMask1);
		mCartBank1[address&mMaskBank1]=data;
	}
	if(!mStrobe)
	{
		mCounter++;
		mCounter&=0x07ff;
	}
}


uint8 CCart::Peek0(void)
{
	uint32 address=(mShifter<<mShiftCount0)+(mCounter&mCountMask0);
	uint8 data=mCartBank0[address&mMaskBank0];

	if(!mStrobe)
	{
		mCounter++;
		mCounter&=0x07ff;
	}

	return data;
}

uint8 CCart::Peek1(void)
{
	uint32 address=(mShifter<<mShiftCount1)+(mCounter&mCountMask1);
	uint8 data=mCartBank1[address&mMaskBank1];

	if(!mStrobe)
	{
		mCounter++;
		mCounter&=0x07ff;
	}

	return data;
}


int CCart::StateAction(StateMem *sm, int load, int data_only)
{
 SFORMAT CartRegs[] =
 {
		SFVAR(mCounter),
		SFVAR(mShifter),
		SFVAR(mAddrData),
		SFVAR(mStrobe),
		SFVAR(mShiftCount0),
		SFVAR(mCountMask0),
		SFVAR(mShiftCount1),
		SFVAR(mCountMask1),
		SFVAR(mBank),
		SFVAR(mWriteEnableBank0),
		SFVAR(mWriteEnableBank1),
	SFVAR(last_strobe),
	SFARRAYN(mCartBank1, mCartRAM ? mMaskBank1 + 1 : 0, "mCartBank1"),
	SFEND
 };
 return MDFNSS_StateAction(sm, load, data_only, CartRegs, "CART", false);
}

