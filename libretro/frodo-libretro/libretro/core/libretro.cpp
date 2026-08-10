#include <stdarg.h>
#include <libretro.h>
#include <compat/strl.h>

#include "libretro-core.h"
#include "libretro_core_options.h"

#ifdef NO_LIBCO
#include "main.h"
#include "C64.h"
#include "Display.h"
#include "Prefs.h"
#else
cothread_t mainThread;
cothread_t emuThread;
#endif

int CROP_WIDTH;
int CROP_HEIGHT;
int VIRTUAL_WIDTH;
int retrow=1024; 
int retroh=1024;

#ifdef NO_LIBCO
extern C64 *TheC64;
extern void quit_frodo_emu(void);
#endif

extern int SHIFTON,pauseg,SND ,snd_sampler;
extern short signed int SNDBUF[1024*2];
extern char RPATH[512];

#include "cmdline.c"

extern void texture_init(void);
extern void texture_uninit(void);

const char *retro_save_directory;
const char *retro_system_directory;
const char *retro_content_directory;

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static void RETRO_CALLCONV fallback_log(enum retro_log_level level, const char *fmt, ...);
retro_log_printf_t log_cb = fallback_log;

static void RETRO_CALLCONV fallback_log(
      enum retro_log_level level, const char *fmt, ...) { }

void retro_set_environment(retro_environment_t cb)
{
   struct retro_log_callback log;
   bool no_rom = true;

   environ_cb = cb;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
     log_cb = log.log;

   libretro_set_core_options(environ_cb);

   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);
}

static void update_variables(void)
{
   struct retro_variable var;
   var.key   = "frodo_resolution";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      strlcpy(str, var.value, sizeof(str));

      pch = strtok(str, "x");
      if (pch)
         retrow = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         retroh = strtoul(pch, NULL, 0);

      //FIXME remove force 384x288
      retrow        = WINDOW_WIDTH;
      retroh        = WINDOW_HEIGHT;
      CROP_WIDTH    = retrow;
      CROP_HEIGHT   = (retroh-80);
      VIRTUAL_WIDTH = retrow;
      texture_init();
      //reset_screen();
   }
}

static void retro_wrap_emulator(void)
{
   pre_main(RPATH);
#ifndef NO_LIBCO
   pauseg=-1;

   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, 0); 

   // Were done here
   co_switch(mainThread);

   // Dead emulator, but libco says not to return
   while(1)
      co_switch(mainThread);
#endif
}

void Emu_init(void)
{
   update_variables();

   memset(Key_Sate,0,512);
   memset(Key_Sate2,0,512);

#ifndef NO_LIBCO
   if(!emuThread && !mainThread)
   {
      mainThread = co_active();
      emuThread = co_create(65536*sizeof(void*), retro_wrap_emulator);
   }
#else
   retro_wrap_emulator();
#endif

}

void Emu_uninit(void)
{
#ifdef NO_LIBCO
   quit_frodo_emu();
#endif
   texture_uninit();
}

void retro_shutdown_core(void)
{
#ifdef NO_LIBCO
	quit_frodo_emu();
#endif
   texture_uninit();
   environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
}

/* TODO/FIXME - implement this */
void retro_reset(void) { }

void retro_init(void)
{    	
   const char *save_dir        = NULL;
   const char *content_dir     = NULL;
   const char *system_dir      = NULL;
#ifndef RENDER16B
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
#else
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
#endif

   // if defined, use the system directory			
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir) && system_dir)
      retro_system_directory = system_dir;		

   // if defined, use the system directory			
   if (environ_cb(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, &content_dir) && content_dir)
      retro_content_directory = content_dir;		

   // If save directory is defined use it, otherwise use system directory
   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) && save_dir)
      retro_save_directory = *save_dir ? save_dir : retro_system_directory;      
   else
   {
      // make retro_save_directory the same in case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY is not implemented by the frontend
      retro_save_directory=retro_system_directory;
   }

   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

	struct retro_input_descriptor inputDescriptors[] = {
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "R2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "L2" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "R3" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "L3" },
		{ 0 }
	};
	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, &inputDescriptors);
#ifndef NO_LIBCO
   Emu_init();
#endif
   texture_init();

}

void retro_deinit(void)
{	 
   Emu_uninit(); 

#ifndef NO_LIBCO
   if(emuThread)
   {	 
      co_delete(emuThread);
      emuThread = 0;
   }
#endif
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "Frodo";
   info->library_version  = "V4_2";
   info->valid_extensions = "d64|t64|x64|p00|lnx|lyx|zip";
   info->need_fullpath    = true;
   info->block_extract = false;

}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
  struct retro_game_geometry geom = {
				     (unsigned int) retrow,
				     (unsigned int) retroh,
				     1024, 1024,4.0 / 3.0 };
   struct retro_system_timing timing = { 50.0, 44100.0 };

   info->geometry = geom;
   info->timing   = timing;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

#ifdef NO_LIBCO
/* TODO/FIXME - nolibco Gui endless loop -> no retro_run() call */
void retro_run_gui(void)
{
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      update_variables();

   video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);
}
#endif

static void (*pulse_handler)(int);

void libretro_pulse_handler(void (*handler)(int))
{
   pulse_handler = handler;
}

void retro_run(void)
{
   static int pulse_counter = 0;
   int x;

   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,
            &updated) && updated)
      update_variables();

   if (pulse_counter > 20 && pulse_handler)
      pulse_handler(0);

   if(pauseg==0)
   {

      if(SND==1)
         for(x=0;x<882;x++)
            audio_cb(SNDBUF[x],SNDBUF[x]);
#ifdef NO_LIBCO
#ifndef FRODO_SC
      for(x=0;x<312;x++)
#else
         for(x=0;x<63*312;x++) 
#endif
            TheC64->thread_func();
#endif
   }   

   video_cb(Retro_Screen,retrow,retroh,retrow<<PIXEL_BYTES);

#ifndef NO_LIBCO   
   co_switch(emuThread);
#endif

}

bool retro_load_game(const struct retro_game_info *info)
{
   const char *full_path = NULL;

#ifndef NO_LIBCO
   if (!mainThread || !emuThread)
   {
      log_cb(RETRO_LOG_ERROR, "libco init failed\n", __LINE__);
      return false;
   }
#endif

   if (info)
      full_path = info->path;

   if (full_path)
     strcpy(RPATH,full_path);
   else
     memset(RPATH, 0, sizeof(RPATH));

   update_variables();

#ifdef RENDER16B
	memset(Retro_Screen,0,1024*1024*2);
#else
	memset(Retro_Screen,0,1024*1024*2*2);
#endif
	memset(SNDBUF,0,1024*2*2);

#ifndef NO_LIBCO
	co_switch(emuThread);
#else
	Emu_init();
#endif
   return true;
}

void retro_unload_game(void)
{
   pauseg=0;
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(
      unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   return 0;
}

void retro_cheat_reset(void) { }

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

