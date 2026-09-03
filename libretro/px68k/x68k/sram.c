/*
 *  SRAM.C - SRAM (16kb)
 */

#include	"common.h"
#include	"../libretro/dosio.h"
#include	"prop.h"
#include	"winx68k.h"
#include	"sysport.h"
#include	"x68kmemory.h"
#include	"sram.h"

static int write_enabled = 0;
uint8_t	SRAM[0x4000];

/* Base dir where sram.dat lives (the save directory, set in retro_init).
 * winx68k_dir (keropi/BIOS dir) is the default base for firmware files; we
 * redirect to save_base_dir only around the sram.dat I/O and restore afterwards. */
extern char save_base_dir[];

int SRAM_StateAction(StateMem *sm, int load, int data_only)
{
	SFORMAT StateRegs[] = 
	{
		SFARRAYN(SRAM, 0x4000, "MEM_SRAM"),
		SFVAR(write_enabled),

		SFEND
	};

	int ret = PX68KSS_StateAction(sm, load, data_only, StateRegs, "X68K_SRAM", false);

	return ret;
}

void SRAM_VirusCheck(void)
{
	if ( (cpu_readmem24_dword(0xed3f60)==0x60000002)
	   &&(cpu_readmem24_dword(0xed0010)==0x00ed3f60) )
	{
		SRAM_Cleanup();
		SRAM_Init();
	}
}

void SRAM_WriteEnable(int enable)
{
	if (write_enabled != enable)
	{
		log_cb(RETRO_LOG_DEBUG, "sram write enable = %d\n", enable);
		write_enabled = enable;
	}
}

void SRAM_Init(void)
{
	int i;
	void *fp;
	int loaded = 0;

	write_enabled = 0;

	for (i=0; i<0x4000; i++)
		SRAM[i] = 0xFF;

	/* SRAM persistence is optional (px68k_save_sram).  When off, the machine
	 * always boots with a fresh SRAM (the IPL re-initialises it) and nothing is
	 * written back -- see SRAM_Cleanup. */
	if (Config.save_sram)
	{
		file_setcd(save_base_dir);   /* sram.dat lives in the save dir, not keropi */
		fp = file_open_c("sram.dat");
		file_setcd(winx68k_dir);     /* restore base dir for the other firmware files */

		if (fp)
		{
			file_lread(fp, SRAM, 0x4000);
			file_close(fp);
			loaded = 1;

#ifndef MSB_FIRST
			for (i=0; i<0x4000; i+=2)
			{
				uint8_t tmp = SRAM[i];
				SRAM[i]     = SRAM[i+1];
				SRAM[i+1]   = tmp;
			}
#endif
		}
	}

	/* Reject an incompatible/corrupt sram.dat -- e.g. one written by an older
	 * build with different byte-order handling, which comes back word-swapped
	 * so its RAM-size field ($ED0008) is garbage.  A valid X68000 image holds
	 * 1..12 MB in whole-MB steps; if it fails, discard the whole image and
	 * start fresh so the machine re-initialises and re-saves it in the current
	 * format.  Without this a stale file can hang the boot (black screen).
	 * SRAM_Read() applies the correct per-endian byte order, so this is the
	 * value the machine actually sees. */
	if (loaded)
	{
		uint32_t rs = ((uint32_t)SRAM_Read(0x08) << 24) |
		              ((uint32_t)SRAM_Read(0x09) << 16) |
		              ((uint32_t)SRAM_Read(0x0A) <<  8) |
		               (uint32_t)SRAM_Read(0x0B);
		if (rs < 0x00100000 || rs > 0x00C00000 || (rs & 0x000FFFFF))
		{
			log_cb(RETRO_LOG_WARN,
			   "[SRAM] incompatible sram.dat (ramsize=0x%08X) -> reinitialising\n",
			   (unsigned)rs);
			for (i=0; i<0x4000; i++)
				SRAM[i] = 0xFF;
		}
	}
}

void SRAM_Cleanup(void)
{
   void *fp;

   if (!Config.save_sram)   /* persistence disabled: keep the machine stateless */
      return;

   /* sram.dat is stored in native 68000 word order (portable LE<->BE).  On
    * little-endian the runtime SRAM[] is byte-swapped, so swap it back before
    * writing; on big-endian SRAM[] is already native (see SRAM_Init), so the
    * swap must be skipped or the persisted file ends up in the wrong order. */
#ifndef MSB_FIRST
   {
      int i;
      for (i=0; i<0x4000; i+=2)
      {
         uint8_t tmp = SRAM[i];
         SRAM[i]     = SRAM[i+1];
         SRAM[i+1]   = tmp;
      }
   }
#endif

   file_setcd(save_base_dir);       /* sram.dat lives in the save dir, not keropi */
   if (!(fp = file_open_c("sram.dat")))
      fp = file_create_c("sram.dat");
   file_setcd(winx68k_dir);    /* restore base dir */

   if (!fp)
      return;

   file_lwrite(fp, SRAM, 0x4000);
   file_close(fp);
}

uint8_t FASTCALL SRAM_Read(uint32_t adr)
{
	adr &= 0xffff;
	/* SRAM is stored natively on big-endian (SRAM_Init/SRAM_Write skip the
	 * byte-swap there), so the read must NOT swap either.  The unconditional
	 * adr^=1 here read the adjacent byte on big-endian -> Human68k saw a
	 * word-swapped RAM size (2MB 0x00200000 -> 0x20000000 = 536MB) and other
	 * corrupted SRAM config, breaking HDD-installed games that allocate from
	 * the reported free memory. */
#ifndef MSB_FIRST
	adr ^= 1;
#endif
	if (adr<0x4000)
		return SRAM[adr];
	return 0xff;
}


void FASTCALL SRAM_Write(uint32_t adr, uint8_t data)
{
	if ( write_enabled && (adr < 0xed4000) )
	{
		adr       &= 0xffff;
#ifndef MSB_FIRST
		adr       ^= 1;
#endif
		SRAM[adr]  = data;
	}
}

void FASTCALL SRAM_UpdateBoot(void)
{
	cpu_writemem24(0xe8e00d, 0x31); /* SRAM write permission */
	cpu_writemem24_dword(0xed0040, cpu_readmem24_dword(0xed0040) + 1); /* Estimated operation time(min.) */
	cpu_writemem24_dword(0xed0044, cpu_readmem24_dword(0xed0044) + 1); /* Estimated booting times */
	cpu_writemem24(0xe8e00d, 0x00);
}
