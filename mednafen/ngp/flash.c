//---------------------------------------------------------------------------
// NEOPOP : Emulator as in Dreamland
//
// Copyright (c) 2001-2002 by neopop_uk
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version. See also the license.txt file for
//	additional informations.
//---------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "flash.h"
#include "mem.h"
#include "rom.h"
#include "system.h"

#include "../state.h"

//-----------------------------------------------------------------------------
// Local Definitions
//-----------------------------------------------------------------------------
//This value is used to verify flash data - it is set to the
//version number that the flash description was modified for.

#define FLASH_VALID_ID		0x0053

//Number of different flash blocks, this should be enough.

#define FLASH_MAX_BLOCKS	256

typedef struct
{
   //Flash Id
   uint16_t valid_flash_id;		// = FLASH_VALID_ID

   uint16_t block_count;			//Number of flash data blocks

   uint32_t total_file_length;		// header + block[0 - block_count]

} FlashFileHeader;

typedef struct
{
	uint32_t start_address;		// 24 bit address
	uint16_t data_length;		// length of following data

	//Followed by data_length bytes of the actual data.

} FlashFileBlockHeader;

/* Local Data */
static FlashFileBlockHeader	blocks[256];
static uint16_t block_count;

void flash_optimise_blocks(void)
{
   int i, j;

   // Bubble Sort by address
   for (i = 0; i < block_count - 1; i++)
   {
      for (j = i+1; j < block_count; j++)
      {
         //Swap?
         if (blocks[i].start_address > blocks[j].start_address)
         {
            uint16_t temp16;
            uint32_t temp32 = blocks[i].start_address;

            blocks[i].start_address = blocks[j].start_address;
            blocks[j].start_address = temp32;

            temp16 = blocks[i].data_length;
            blocks[i].data_length = blocks[j].data_length;
            blocks[j].data_length = temp16;
         }
      }
   }

   //Join contiguous blocks
   //Only advance 'i' if required, this will allow subsequent
   //blocks to be compared to the newly expanded block.
   for (i = 0; i < block_count - 1; /**/)
   {
      //Next block lies within (or borders) this one?
      if (blocks[i+1].start_address <=
            (blocks[i].start_address + blocks[i].data_length))
      {
         //Extend the first block
         blocks[i].data_length = 
            (uint16_t)((blocks[i+1].start_address + blocks[i+1].data_length) - 
                  blocks[i].start_address);
         //FIXME: std::max

         //Remove the next one.
         for (j = i+2; j < block_count; j++)
         {
            blocks[j-1].start_address = blocks[j].start_address;
            blocks[j-1].data_length = blocks[j].data_length;
         }
         block_count --;
      }
      else
      {
         i++;	// Try the next block
      }
   }
}

void do_flash_read(uint8_t *flashdata)
{
   FlashFileHeader header;
   uint8_t *fileptr;
   uint16_t i;
   uint32_t j;
   bool PREV_memory_unlock_flash_write = memory_unlock_flash_write; // kludge, hack, FIXME

   memcpy(&header, flashdata, sizeof(header));

   //Read header
   block_count = header.block_count;
   fileptr = flashdata + sizeof(FlashFileHeader);

   //Copy blocks
   memory_unlock_flash_write = 1;
   for (i = 0; i < block_count; i++)
   {
      FlashFileBlockHeader* current = (FlashFileBlockHeader*)fileptr;
      fileptr += sizeof(FlashFileBlockHeader);

      blocks[i].start_address = current->start_address;
      blocks[i].data_length = current->data_length;

      //Copy data
      for (j = 0; j < blocks[i].data_length; j++)
      {
         storeB(blocks[i].start_address + j, *fileptr);
         fileptr++;
      }
   }
   memory_unlock_flash_write = PREV_memory_unlock_flash_write;

   flash_optimise_blocks();		//Optimise
}

void flash_read(void)
{
   FlashFileHeader header;
   uint8_t* flashdata;

   //Initialise the internal flash configuration
   block_count              = 0;

   header.valid_flash_id    = 0;
   header.block_count       = 0;
   header.total_file_length = 0;

   //Read flash buffer header
   if (system_io_flash_read((uint8_t*)&header, sizeof(FlashFileHeader)) == 0)
      return; //Silent failure - no flash data yet.

   //Verify correct flash id
   if (header.valid_flash_id != FLASH_VALID_ID)
      return;

   //Read the flash data
   flashdata = (uint8_t*)malloc(header.total_file_length * sizeof(uint8_t));
   system_io_flash_read(flashdata, header.total_file_length);

   do_flash_read(flashdata);

   free(flashdata);
}

void flash_write(uint32_t start_address, uint16_t length)
{
   uint16_t i;

   //Now we need a new flash command before the next flash write will work!
   memory_flash_command = false;

   for (i = 0; i < block_count; i++)
   {
      //Got this block with enough bytes to cover it
      if (blocks[i].start_address == start_address &&
            blocks[i].data_length >= length)
         return; //Nothing to do, block already registered.

      //Got this block with but it's length is too short
      if (blocks[i].start_address == start_address &&
            blocks[i].data_length < length)
      {
         blocks[i].data_length = length;	//Enlarge block updating.
         return;
      }
   }

   // New block needs to be added
   blocks[block_count].start_address = start_address;
   blocks[block_count].data_length = length;
   block_count++;
}

uint8_t *make_flash_commit(int32_t *length)
{
   int i;
   FlashFileHeader header;
   uint8_t *flashdata, *fileptr;

   /* No flash data? */
   if (block_count == 0)
      return NULL;

   /* Optimize before writing */
   flash_optimise_blocks();

   /* Build a header */
   header.valid_flash_id    = FLASH_VALID_ID;
   header.block_count       = block_count;
   header.total_file_length = sizeof(FlashFileHeader);

   for (i = 0; i < block_count; i++)
   {
      header.total_file_length += sizeof(FlashFileBlockHeader);
      header.total_file_length += blocks[i].data_length;
   }

   /* Write the flash data */
   flashdata = (uint8_t*)malloc(header.total_file_length * sizeof(uint8_t));

   /* Copy header */
   memcpy(flashdata, &header, sizeof(FlashFileHeader));
   fileptr = flashdata + sizeof(FlashFileHeader);

   /* Copy blocks */
   for (i = 0; i < block_count; i++)
   {
      uint32_t j;

      memcpy(fileptr, &blocks[i], sizeof(FlashFileBlockHeader));
      fileptr += sizeof(FlashFileBlockHeader);

      /* Copy data */
      for (j = 0; j < blocks[i].data_length; j++)
      {
         *fileptr = loadB(blocks[i].start_address + j);
         fileptr++;
      }
   }

   *length = header.total_file_length;
   return flashdata;
}

void flash_commit(void)
{
   int32_t length = 0;
   uint8_t *flashdata = make_flash_commit(&length);

   if (!flashdata)
      return;

   system_io_flash_write(flashdata, length);
   free(flashdata);
}
