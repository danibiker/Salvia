/*
 * Copyright (c) 2003,2008 NONAKA Kimihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* -------------------------------------------------------------------------- *
 *  WINUI.C - UI                                                              *
 * -------------------------------------------------------------------------- */

#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <strings.h>

#include "common.h"
#include "keyboard.h"
#include "windraw.h"
#include "dswin.h"
#include "dosio.h"
#include "prop.h"
#include "status.h"
#include "joystick.h"
#include "mouse.h"
#include "winx68k.h"
#include "fdd.h"
#include "irqh.h"
#include "../m68000/m68000.h"
#include "crtc.h"
#include "mfp.h"
#include "fdc.h"
#include "disk_d88.h"
#include "dmac.h"
#include "ioc.h"
#include "rtc.h"
#include "sasi.h"
#include "bg.h"
#include "palette.h"
#include "crtc.h"
#include "pia.h"
#include "scc.h"
#include "midi.h"
#include "adpcm.h"
#include "mercury.h"
#include "tvram.h"
#include "winui.h"

#include "fmg_wrap.h"

#include <file/file_path.h>

#ifdef _WIN32
#define SLASH '\\'
#else
#define SLASH '/'
#endif

uint8_t	Debug_Text=1, Debug_Grp=1, Debug_Sp=1;

char		filepath[MAX_PATH] = ".";

char     cur_dir_str[MAX_PATH];
size_t   cur_dir_slen;

int menu_state = MS_KEY;
int mkey_y     = 0;
int mkey_pos   = 0;

struct menu_flist mfl;

extern char base_dir[MAX_PATH];

/***** menu items *****/

#define MENU_NUM 5
#define MENU_WINDOW 7

int mval_y[] = {
	0,
	0,
	0,
	0,
	0
};

enum menu_id
{
	M_SYS,
	M_FD0,
	M_FD1,
	M_HD0,
	M_HD1
};

/* Max # of characters is 15. */
char menu_item_key[][15] = {
	"SYSTEM",
	"FDD0",
	"FDD1",
	"HDD0",
	"HDD1",
	"",
	"",
	"uhyo",
	""
};

/* Max # of characters is 30. */
/* Max # of items including terminater `""' in each line is 15. */
char menu_items[][15][30] = {
	{"RESET", "NMI RESET", "QUIT", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""},
	{"dummy", "EJECT", ""}
};

static void menu_system(int v)
{
	switch (v)
   {
      case 0 :
         WinX68k_Reset();
         break;
      case 1:
         IRQH_Int(7, NULL);
         break;
   }
}

static void upper(char *s)
{
	while (*s != '\0')
   {
      if (*s >= 'a' && *s <= 'z')
         *s = 'A' + *s - 'a';
      s++;
   }
}

static void switch_mfl(int a, int b)
{
   /* exchange 2 values in mfl list */
   char name_tmp[MAX_PATH];
   char type_tmp;

   strcpy(name_tmp, mfl.name[a]);
   type_tmp    = mfl.type[a];

   strcpy(mfl.name[a], mfl.name[b]);
   mfl.type[a] = mfl.type[b];

   strcpy(mfl.name[b], name_tmp);
   mfl.type[b] = type_tmp;
}

#ifdef USE_LIBRETRO_VFS
static void menu_create_flist(int v)
{
	int drv;
	int i, a;
	DIRH *dp;

	/* file extension of FD image */
	char support[] = "D8888DHDMDUP2HDDIMXDFIMG";

	drv = WinUI_get_drv_num(mkey_y);

	if (drv < 0)
	{
		return;
	}

	/* set current directory when FDD is ejected */
	if (v == 1)
	{
		if (drv < 2)
		{
			FDD_EjectFD(drv);
			Config.FDDImage[drv][0] = '\0';
		}
		else
		{
			Config.HDImage[drv - 2][0] = '\0';
		}
		/* retain last directory used upon eject */
      /* strcpy(mfl.dir[drv], cur_dir_str); */
		return;
	}

	if (drv >= 2)
	{
		strcpy(support, "HDF");
	}

	/* This routine gets file lists. */
	dp = wrap_vfs_opendir(mfl.dir[drv]);

	/* xxx check if dp is null... */
	if (!dp)
	{
		char tmp[PATH_MAX];
		/* failed to open StartDir, use rom folder as default */
		/* TODO: check for path more early */
		sprintf(tmp, "%s%c", base_dir, SLASH);
		strcpy(mfl.dir[drv], tmp);
		/* re-open folder */
		dp = wrap_vfs_opendir(mfl.dir[drv]);
	}

	/* xxx You can get only MFL_MAX files. */
	for (i = 0; i < MFL_MAX; i++)
	{
		char ent_name[MAX_PATH];
		const char *n;
		int st_mode = 0;
		int st_size = 0;

		if (!(wrap_vfs_readdir(dp)))
			break;

		n = dp->d_name;
		strcpy(ent_name, mfl.dir[drv]);
		strcat(ent_name, n);
		st_mode = wrap_vfs_stat(ent_name, &st_size);

		if (!(st_mode & STAT_IS_DIRECTORY))
		{
			char *p;
			char ext[4];
			int len = strlen(n);

			/* Check extension if this is file. */
			if (len < 4 || *(n + len - 4) != '.')
			{
				i--;
				continue;
			}
			strcpy(ext, n + len - 3);
			upper(ext);
			p = strstr(support, ext);
			if (p == NULL || (p - support) % 3 != 0)
			{
				i--;
				continue;
			}
		}
		else
		{
			if (!strcmp(n, "."))
			{
				i--;
				continue;
			}

			/* You can't go up over current directory. */
			if (!strcmp(n, "..") && !strcmp(mfl.dir[drv], cur_dir_str))
			{
				i--;
				continue;
			}
		}

		strcpy(mfl.name[i], n);
		/* set 1 if this is directory */
		mfl.type[i] = (st_mode & STAT_IS_DIRECTORY) ? 1 : 0;
#ifdef DEBUG
		p6logd("%s 0x%x\n", n, buf.st_mode);
#endif
	}

	wrap_vfs_closedir(dp);

	strcpy(mfl.name[i], "");
	mfl.num = i;
	mfl.ptr = 0;

	/* Sorting mfl!
	 * Folder first, than files
	 * buble sort glory */
	for (a = 0; a < i - 1; a++)
	{
		int b;
		for (b = a + 1; b < i; b++)
		{
			if (mfl.type[a] < mfl.type[b])
				switch_mfl(a, b);
			if ((mfl.type[a] == mfl.type[b]) && (strcasecmp(mfl.name[a], mfl.name[b]) > 0))
				switch_mfl(a, b);
		}
	}
}

#else /* !USE_LIBRETRO_VFS */

#include <dirent.h>
#include <sys/stat.h>
static void menu_create_flist(int v)
{
   DIR *dp;
   int i, len;
   int a, b;
   struct dirent *dent;
   struct stat buf;
   char *n, ext[4], *p;
   char ent_name[MAX_PATH];
   /* file extension of FD image */
   char support[] = "D8888DHDMDUP2HDDIMXDFIMG";
   int drv        = WinUI_get_drv_num(mkey_y);

   if (drv < 0)
      return;

   /* set current directory when FDD is ejected */
   if (v == 1)
   {
      if (drv < 2)
      {
         FDD_EjectFD(drv);
         Config.FDDImage[drv][0] = '\0';
      }
      else
         Config.HDImage[drv - 2][0] = '\0';
      /* retain last directory used upon eject */
      /* strcpy(mfl.dir[drv], cur_dir_str); */
      return;
   }

   if (drv >= 2)
      strcpy(support, "HDF");

   /* This routine gets file lists. */
   dp = opendir(mfl.dir[drv]);

   /* xxx check if dp is null... */
   if (!dp)
   {
      char tmp[PATH_MAX];
      /* failed to open StartDir, use rom folder as default */
      /* TODO: check for path more early */
      snprintf(tmp, sizeof(tmp), "%s%c", base_dir, SLASH);
      strcpy(mfl.dir[drv], tmp);
      /* re-open folder */
      dp = opendir(mfl.dir[drv]);
   }

   /* xxx You can get only MFL_MAX files. */
   for (i = 0 ; i < MFL_MAX; i++)
   {
      dent = readdir(dp);
      if (!dent)
         break;
      n = dent->d_name;
      strcpy(ent_name, mfl.dir[drv]);
      strcat(ent_name, n);
      stat(ent_name, &buf);

      if (!S_ISDIR(buf.st_mode))
      {
         /* Check extension if this is file. */
         len = strlen(n);
         if (len < 4 || *(n + len - 4) != '.')
         {
            i--;
            continue;
         }
         strcpy(ext, n + len - 3);
         upper(ext);
         p = strstr(support, ext);
         if (!p || (p - support) % 3 != 0)
         {
            i--;
            continue;
         }
      }
      else
      {
         if (!strcmp(n, "."))
         {
            i--;
            continue;
         }

         /* You can't go up over current directory. */
         if (     !strcmp(n, "..")
               && !strcmp(mfl.dir[drv], cur_dir_str))
         {
            i--;
            continue;
         }
      }

      strcpy(mfl.name[i], n);
      /* set 1 if this is directory */
      mfl.type[i] = S_ISDIR(buf.st_mode)? 1 : 0;
   }

   closedir(dp);

   strcpy(mfl.name[i], "");
   mfl.num = i;
   mfl.ptr = 0;

   /* Sorting mfl!
    * Folder first, than files
    * buble sort glory */
   for (a=0; a<i-1; a++)
   {
      for (b=a+1; b<i; b++)
      {
         if (mfl.type[a]<mfl.type[b])
            switch_mfl(a, b);
         if (     (mfl.type[a] == mfl.type[b]) 
               && (strcasecmp(mfl.name[a], mfl.name[b]) > 0))
            switch_mfl(a, b);
      }
   }
}

#endif

struct _menu_func {
	void (*func)(int v);
	int imm;
};

struct _menu_func menu_func[] = {
	{menu_system, 0},
	{menu_create_flist, 0},
	{menu_create_flist, 0},
	{menu_create_flist, 0},
	{menu_create_flist, 0}
};

int WinUI_get_drv_num(int key)
{
	char *s = menu_item_key[key];

	if (!strncmp("FDD", s, 3))
		return strcmp("FDD0", s)?
			(strcmp("FDD1", s)? -1 : 1) : 0;
	return strcmp("HDD0", s)?
		(strcmp("HDD1", s)? -1: 3) : 2;
}

/* TODO/FIXME - we need a way of iterating over all drives for Windows */
#ifdef _WIN32
#define CUR_DIR_STR "c:\\"
#else
#define CUR_DIR_STR "/"
#endif

/******************************************************************************
 * init
 ******************************************************************************/
void plusyen(char *s, size_t len);
void WinUI_Init(void)
{
	int i;

   if(filepath[0] != 0)
      strcpy(cur_dir_str, filepath);
   else
      strcpy(cur_dir_str, CUR_DIR_STR);

   plusyen(cur_dir_str, sizeof(cur_dir_str));
   cur_dir_slen = strlen(cur_dir_str);

   for (i = 0; i < 4; i++)
	   strcpy(mfl.dir[i], cur_dir_str);
}

/* ex. ./hoge/.. -> ./
 * ( ./ ---down hoge dir--> ./hoge ---up hoge dir--> ./hoge/.. ) */
static void shortcut_dir(int drv)
{
   int i, found = 0;
   /* len is larger than 2 */
   size_t len = strlen(mfl.dir[drv]);
   char *p    = mfl.dir[drv] + len - 2;
   for (i = len - 2; i >= 0; i--)
   {
      if (*p == SLASH/*'/'*/)
      {
         found = 1;
         break;
      }
      p--;
   }
#ifdef _WIN32
   if (found && strcmp(p, "\\..\\"))
#else
      if (found && strcmp(p, "/../"))
#endif
         *(p + 1) = '\0';
      else
#ifdef _WIN32
         strcat(mfl.dir[drv], "..\\");
#else
   strcat(mfl.dir[drv], "../");
#endif
}

int WinUI_Menu(int first)
{
   int i, n, ii;
   int cursor0;
   uint8_t joy;
   int menu_redraw  = 0;
   int pad_changed  = 0;
   int mfile_redraw = 0;

   if (first)
   {
      menu_state    = MS_KEY;
      mkey_y        = 0;
      mkey_pos      = 0;
      menu_redraw   = 1;
      first         = 0;
      /*  The screen is not rewritten without any key actions,
       * so draw screen first. */
      WinDraw_ClearMenuBuffer();
      WinDraw_DrawMenu(menu_state, mkey_pos, mkey_y, mval_y);
   }

   cursor0 = mkey_y;
   joy     = get_joy_downstate();
   reset_joy_downstate();

   if (!(joy & JOY_UP))
   {
      switch (menu_state)
      {
         case MS_KEY:
            if (mkey_y > 0)
               mkey_y--;
            if (mkey_pos > mkey_y)
               mkey_pos--;
            break;
         case MS_VALUE:
            if (mval_y[mkey_y] > 0)
            {
               mval_y[mkey_y]--;

               /* do something immediately */
               if (menu_func[mkey_y].imm)
                  menu_func[mkey_y].func(mval_y[mkey_y]);

               menu_redraw = 1;
            }
            break;
         case MS_FILE:
            if (mfl.y == 0)
            {
               if (mfl.ptr > 0)
                  mfl.ptr--;
            }
            else
               mfl.y--;
            mfile_redraw = 1;
            break;
      }
   }

   if (!(joy & JOY_DOWN))
   {
      switch (menu_state)
      {
         case MS_KEY:
            if (mkey_y < MENU_NUM - 1)
               mkey_y++;
            if (mkey_y > mkey_pos + MENU_WINDOW - 1)
               mkey_pos++;
            break;
         case MS_VALUE:
            if (menu_items[mkey_y][mval_y[mkey_y] + 1][0] != '\0')
            {
               mval_y[mkey_y]++;

               if (menu_func[mkey_y].imm)
                  menu_func[mkey_y].func(mval_y[mkey_y]);

               menu_redraw = 1;
            }
            break;
         case MS_FILE:
            if (mfl.y == 13)
            {
               if (mfl.ptr + 14 < mfl.num
                     && mfl.ptr < MFL_MAX - 13)
                  mfl.ptr++;
            }
            else if (mfl.y + 1 < mfl.num)
               mfl.y++;
            mfile_redraw = 1;
            break;
      }
   }

   if (!(joy & JOY_LEFT))
   {
      switch (menu_state)
      {
         case MS_KEY:
            break;
         case MS_VALUE:
            if (mval_y[mkey_y] > 0)
            {
               mval_y[mkey_y]-=10;
               if (mval_y[mkey_y]<0)
                  mval_y[mkey_y] = 0;

               /* do something immediately */
               if (menu_func[mkey_y].imm)
                  menu_func[mkey_y].func(mval_y[mkey_y]);

               menu_redraw = 1;
            }
            break;
         case MS_FILE:
            if (mfl.y == 0)
            {
               if (mfl.ptr > 0)
               {
                  mfl.ptr-=10;
                  if (mfl.ptr < 0 )
                     mfl.ptr = 0;
               }
            }
            else
            {
               mfl.y-=10;
               if (mfl.y<0)
               {
                  if (mfl.ptr > 0)
                     mfl.ptr += mfl.y;
                  if (mfl.ptr < 0)
                     mfl.ptr = 0;
                  mfl.y = 0;
               }
            }
            mfile_redraw = 1;
            break;
      }
   }

   if (!(joy & JOY_RIGHT))
   {
      switch (menu_state)
      {
         case MS_KEY:
            break;
         case MS_VALUE:
            for (ii = 0; ii<10; ii++)
            {
               if (menu_items[mkey_y][mval_y[mkey_y] + 1][0] != '\0')
               {
                  mval_y[mkey_y]++;

                  if (menu_func[mkey_y].imm)
                     menu_func[mkey_y].func(mval_y[mkey_y]);

                  menu_redraw = 1;
               }
            }
            break;
         case MS_FILE:
            for (ii=0; ii<10; ii++)
            {
               if (mfl.y == 13)
               {
                  if (mfl.ptr + 14 < mfl.num
                        && mfl.ptr < MFL_MAX - 13)
                     mfl.ptr++;
               } else if (mfl.y + 1 < mfl.num)
                  mfl.y++;
               mfile_redraw = 1;
            }
            break;
      }
   }

   if (!(joy & JOY_TRG1))
   {
      int drv, y;
      switch (menu_state)
      {
         case MS_KEY:
            menu_state  = MS_VALUE;
            menu_redraw = 1;
            break;
         case MS_VALUE:
            menu_func[mkey_y].func(mval_y[mkey_y]);

            if (menu_state == MS_HWJOY_SET)
            {
               menu_redraw = 1;
               break;
            }

            /* get back key_mode if value is set.
             * go file_mode if value is filename. */
            menu_state = MS_KEY;
            menu_redraw = 1;

            drv = WinUI_get_drv_num(mkey_y);
            if (drv >= 0)
            {
               if (mval_y[mkey_y] == 0)
               {
                  /* go file_mode */
                  menu_state     = MS_FILE;
                  menu_redraw    = 0; /* reset */
                  mfile_redraw   = 1;
               }
               else 
                  mval_y[mkey_y] = 0;
            }
            else if (!strcmp("SYSTEM", menu_item_key[mkey_y]))
            {
               if (mval_y[mkey_y] == 2)
                  return WUM_EMU_QUIT;
               return WUM_MENU_END;
            }
            break;
         case MS_FILE:
            if ((drv = WinUI_get_drv_num(mkey_y)) < 0)
               break;
            y = mfl.ptr + mfl.y;
            /* file loaded */
            if (mfl.type[y])
            {
               /* directory operation */
               if (!strcmp(mfl.name[y], ".."))
               {
                  shortcut_dir(drv);
                  mfl.stack[0][mfl.stackptr] = mfl.stack[1][mfl.stackptr] = 0;
                  mfl.stackptr               = (mfl.stackptr - 1) % 256;
                  mfl.ptr                    = mfl.stack[0][mfl.stackptr];
                  mfl.y                      = mfl.stack[1][mfl.stackptr];
               }
               else
               {
                  strncat(mfl.dir[drv], mfl.name[y], sizeof(mfl.dir[drv]) - 1);
#ifdef _WIN32
                  strcat(mfl.dir[drv], "\\");
#else
                  strcat(mfl.dir[drv], "/");
#endif
                  mfl.stack[0][mfl.stackptr] = mfl.ptr;
                  mfl.stack[1][mfl.stackptr] = mfl.y;
                  mfl.stackptr = (mfl.stackptr + 1) % 256;
                  mfl.ptr = 0;
                  mfl.y = 0;
               }
               menu_func[mkey_y].func(0);
               mfile_redraw = 1;
            } else {
               /* file operation */
               if (strlen(mfl.name[y]) != 0)
               {
                  char tmpstr[MAX_PATH];
                  strcpy(tmpstr, mfl.dir[drv]);
                  strcat(tmpstr, mfl.name[y]);
                  if (drv < 2)
                  {
                     FDD_SetFD(drv, tmpstr, 0);
                     strcpy(Config.FDDImage[drv], tmpstr);
                  }
                  else
                     strcpy(Config.HDImage[drv - 2], tmpstr);
               }
               menu_state  = MS_KEY;
               menu_redraw = 1;
            }
            mfl.ptr = mfl.stack[0][mfl.stackptr];
            mfl.y = mfl.stack[1][mfl.stackptr];
            break;
         case MS_HWJOY_SET:
            /* Go back keymode
             * if TRG1 of v-pad or hw keyboard was pushed. */
            if (!pad_changed)
            {
               menu_state  = MS_KEY;
               menu_redraw = 1;
            }
            break;
      }
   }

   if (!(joy & JOY_TRG2))
   {
      switch (menu_state)
      {
         case MS_FILE:
            menu_state  = MS_VALUE;
            /* reset position of file cursor */
            mfl.y       = 0;
            mfl.ptr     = 0;
            menu_redraw = 1;
            break;
         case MS_VALUE:
            menu_state  = MS_KEY;
            menu_redraw = 1;
            break;
         case MS_HWJOY_SET:
            /* Go back keymode
             * if TRG2 of v-pad or hw keyboard was pushed. */
            if (!pad_changed)
            {
               menu_state  = MS_KEY;
               menu_redraw = 1;
            }
            break;
      }
   }

   if (pad_changed)
      menu_redraw = 1;

   if (cursor0 != mkey_y)
      menu_redraw = 1;

   if (mfile_redraw)
   {
      WinDraw_DrawMenufile(&mfl);
      mfile_redraw = 0;
   }

   if (menu_redraw)
   {
      WinDraw_ClearMenuBuffer();
      WinDraw_DrawMenu(menu_state, mkey_pos, mkey_y, mval_y);
   }

   return 0;
}
