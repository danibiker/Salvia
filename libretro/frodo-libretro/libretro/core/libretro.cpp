#include <stdarg.h>
#include <libretro.h>
#include <compat/strl.h>
#include <streams/file_stream.h>
#include <vector>
#include <string>

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

/* Auto-start / disk program selector state (defined in Src/Display.cpp) */
extern int autostart_enabled;
extern int autostart_countdown;
extern int autostart_mode;
extern int SHOWLIST;
extern int show_drive_leds;

/* Core-option mirror + apply (defined in Src/main.cpp) */
extern int fopt_sid_engine, fopt_sid_filters, fopt_true_drive, fopt_fast_reset;
extern int fopt_joy_port, fopt_sprite_coll, fopt_reu;
extern void frodo_apply_prefs(void);

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

   var.key   = "frodo_autostart";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      autostart_enabled = (strcmp(var.value, "disabled") != 0);

   var.key   = "frodo_drive_leds";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      show_drive_leds = (strcmp(var.value, "enabled") == 0);

   var.key   = "frodo_sid_engine";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      fopt_sid_engine = (strcmp(var.value, "none") == 0) ? 0 : 1;

   var.key   = "frodo_sid_filters";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      fopt_sid_filters = (strcmp(var.value, "disabled") == 0) ? 0 : 1;

   var.key   = "frodo_true_drive";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      fopt_true_drive = (strcmp(var.value, "enabled") == 0) ? 1 : 0;

   var.key   = "frodo_fast_reset";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      fopt_fast_reset = (strcmp(var.value, "enabled") == 0) ? 1 : 0;

   var.key   = "frodo_joystick_port";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      fopt_joy_port = (strcmp(var.value, "1") == 0) ? 1 : 2;

   var.key   = "frodo_sprite_collisions";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      fopt_sprite_coll = (strcmp(var.value, "disabled") == 0) ? 0 : 1;

   var.key   = "frodo_reu";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if      (strcmp(var.value, "128K") == 0) fopt_reu = 1;
      else if (strcmp(var.value, "256K") == 0) fopt_reu = 2;
      else if (strcmp(var.value, "512K") == 0) fopt_reu = 3;
      else                                     fopt_reu = 0;
   }

   frodo_apply_prefs();
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

/* ------------------------------------------------------------------ *
 *  Disk control interface (libretro) + .m3u multi-disk support.
 *  Lets the frontend (Salvia's disk menu / "Next CD") swap the disk in
 *  drive 8 at runtime WITHOUT resetting the C64. The live mount is done
 *  by disk_change_image() (Src/main.cpp) via C64::NewPrefs().
 * ------------------------------------------------------------------ */
extern void disk_change_image(const char *path);
extern void quit_frodo_emu(void);   /* idempotent emulator teardown (Src/main.cpp) */

extern "C" {
   RFILE  *rfopen(const char *path, const char *mode);
   int64_t rfseek(RFILE *stream, int64_t offset, int origin);
   int64_t rftell(RFILE *stream);
   int64_t rfread(void *buffer, size_t elem_size, size_t elem_count, RFILE *stream);
   int     rfclose(RFILE *stream);
}

static std::vector<std::string> disk_paths;      // image list (1, or many for .m3u)
static unsigned disk_index         = 0;          // currently selected image
static unsigned disk_initial_index = 0;          // requested by set_initial_image
static bool     disk_ejected       = false;

static bool path_is_m3u(const char *path)
{
   size_t n = path ? strlen(path) : 0;
   char a, b, c;
   if (n < 4 || path[n-4] != '.') return false;
   a = path[n-3]; b = path[n-2]; c = path[n-1];
   if (a >= 'A' && a <= 'Z') a += 32;
   if (b >= 'A' && b <= 'Z') b += 32;
   if (c >= 'A' && c <= 'Z') c += 32;
   return (a == 'm' && b == '3' && c == 'u');
}

/* Parse an .m3u playlist into disk_paths (one image path per line, '#'
   comments skipped, relative entries resolved against the .m3u folder). */
static void disk_load_m3u(const char *path)
{
   RFILE *f = rfopen(path, "rb");
   int64_t sz;
   char *buf, *line;
   std::string base;
   size_t s;

   if (!f) return;
   rfseek(f, 0, SEEK_END);
   sz = rftell(f);
   rfseek(f, 0, SEEK_SET);
   if (sz <= 0 || sz > (1 << 20)) { rfclose(f); return; }
   buf = (char *)malloc((size_t)sz + 1);
   if (!buf) { rfclose(f); return; }
   rfread(buf, 1, (size_t)sz, f);
   rfclose(f);
   buf[sz] = 0;

   base = path;
   s    = base.find_last_of("/\\");
   base = (s == std::string::npos) ? std::string("") : base.substr(0, s + 1);

   line = strtok(buf, "\r\n");
   while (line)
   {
      size_t L;
      while (*line == ' ' || *line == '\t') line++;
      L = strlen(line);
      while (L > 0 && (line[L-1] == ' ' || line[L-1] == '\t')) line[--L] = 0;
      if (L > 0 && line[0] != '#')
      {
         bool absolute = (line[0] == '/' || line[0] == '\\' ||
                          (L >= 2 && line[1] == ':'));
         disk_paths.push_back(absolute ? std::string(line) : base + line);
      }
      line = strtok(NULL, "\r\n");
   }
   free(buf);
}

/* Build the image list for a freshly loaded game and return the path of the
   disk that should be mounted at boot. */
static const char *disk_build_list(const char *full_path)
{
   disk_paths.clear();
   disk_ejected = false;

   if (full_path && full_path[0])
   {
      if (path_is_m3u(full_path))
         disk_load_m3u(full_path);
      if (disk_paths.empty())
         disk_paths.push_back(full_path);   // single image (or unreadable .m3u)
   }

   disk_index = (disk_initial_index < disk_paths.size()) ? disk_initial_index : 0;

   if (!disk_paths.empty())
      return disk_paths[disk_index].c_str();
   return full_path;
}

/* ---- disk_control callbacks (called by the frontend) ---- */
static bool RETRO_CALLCONV dc_set_eject_state(bool ejected)
{
   disk_ejected = ejected;
   /* Mount happens on the eject->insert transition, matching the standard
      eject(true) -> set_image_index() -> eject(false) swap sequence. */
   if (!ejected && disk_index < disk_paths.size() && !disk_paths[disk_index].empty())
      disk_change_image(disk_paths[disk_index].c_str());
   return true;
}
static bool     RETRO_CALLCONV dc_get_eject_state(void) { return disk_ejected; }
static unsigned RETRO_CALLCONV dc_get_image_index(void) { return disk_index; }
static unsigned RETRO_CALLCONV dc_get_num_images(void)  { return (unsigned)disk_paths.size(); }

static bool RETRO_CALLCONV dc_set_image_index(unsigned index)
{
   disk_index = index;
   if (!disk_ejected && index < disk_paths.size() && !disk_paths[index].empty())
      disk_change_image(disk_paths[index].c_str());
   return true;
}
static bool RETRO_CALLCONV dc_replace_image_index(unsigned index, const struct retro_game_info *info)
{
   if (index >= disk_paths.size()) return false;
   disk_paths[index] = (info && info->path) ? std::string(info->path) : std::string();
   return true;
}
static bool RETRO_CALLCONV dc_add_image_index(void)
{
   disk_paths.push_back(std::string());
   return true;
}
static bool RETRO_CALLCONV dc_set_initial_image(unsigned index, const char *path)
{
   (void)path;
   disk_initial_index = index;
   return true;
}
static bool RETRO_CALLCONV dc_get_image_path(unsigned index, char *path, size_t len)
{
   if (index >= disk_paths.size() || disk_paths[index].empty() || !path || !len)
      return false;
   strncpy(path, disk_paths[index].c_str(), len);
   path[len-1] = 0;
   return true;
}
static bool RETRO_CALLCONV dc_get_image_label(unsigned index, char *label, size_t len)
{
   size_t s;
   if (index >= disk_paths.size() || disk_paths[index].empty() || !label || !len)
      return false;
   {
      const std::string &p = disk_paths[index];
      s = p.find_last_of("/\\");
      std::string b = (s == std::string::npos) ? p : p.substr(s + 1);
      strncpy(label, b.c_str(), len);
      label[len-1] = 0;
   }
   return true;
}

static void init_disk_control(void)
{
   unsigned dc_version = 0;
   struct retro_disk_control_callback dcc;

   dcc.set_eject_state     = dc_set_eject_state;
   dcc.get_eject_state     = dc_get_eject_state;
   dcc.get_image_index     = dc_get_image_index;
   dcc.set_image_index     = dc_set_image_index;
   dcc.get_num_images      = dc_get_num_images;
   dcc.replace_image_index = dc_replace_image_index;
   dcc.add_image_index     = dc_add_image_index;

   if (environ_cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &dc_version)
         && dc_version >= 1)
   {
      struct retro_disk_control_ext_callback dcec;
      memset(&dcec, 0, sizeof(dcec));
      dcec.set_eject_state     = dc_set_eject_state;
      dcec.get_eject_state     = dc_get_eject_state;
      dcec.get_image_index     = dc_get_image_index;
      dcec.set_image_index     = dc_set_image_index;
      dcec.get_num_images      = dc_get_num_images;
      dcec.replace_image_index = dc_replace_image_index;
      dcec.add_image_index     = dc_add_image_index;
      dcec.set_initial_image   = dc_set_initial_image;
      dcec.get_image_path      = dc_get_image_path;
      dcec.get_image_label     = dc_get_image_label;
      environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &dcec);
   }
   else
      environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &dcc);
}

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

   init_disk_control();
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
   /* Also forget the main cothread handle (co_active() is not freed, just
      dropped) so Emu_init's re-init guard passes on the next load. Without
      this, emuThread stays NULL on the 2nd load -> "libco init failed". */
   mainThread = 0;

   /* Free the emulator objects that pre_main/skel_main allocated. On reload
      the emu thread is parked inside TheC64->Run(), so co_delete() abandons it
      and ReadyToRun/skel_main never reach their delete calls -> without this,
      each load leaks a C64 (RAM+ROMs), the app object and the display surface.
      Idempotent, so it is a no-op if F10-quit already tore things down. */
   quit_frodo_emu();
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
   info->valid_extensions = "d64|t64|x64|p00|lnx|lyx|zip|prg|m3u";
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
      {
         /* Push the whole frame in ONE batch call. The frontend's batch path
            does proper audio sync (blocking / dynamic rate control); the
            per-sample audio_cb path does a plain non-blocking write with NO
            rate sync, which overruns/underruns the audio ring buffer and
            crackles (very audible on Xbox 360, even during SID silence). */
         static int16_t audio_frame[882 * 2];
         for(x=0;x<882;x++)
         {
            audio_frame[2*x]     = SNDBUF[x];
            audio_frame[2*x + 1] = SNDBUF[x];
         }
         if (audio_batch_cb)
            audio_batch_cb(audio_frame, 882);
      }
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

   /* Build the disk image list (single image, or an .m3u playlist) and pick
      the disk to boot. disk_initial_index may have been set by the frontend
      via set_initial_image() before this call. */
   {
      const char *boot_disk = disk_build_list(full_path);
      if (boot_disk && boot_disk[0])
         strcpy(RPATH, boot_disk);
      else
         memset(RPATH, 0, sizeof(RPATH));
   }

   update_variables();

   /* Arm auto-start for this load. At boot the core reads the mounted disk's
      directory and either auto-runs the single program, or (if there are
      several) opens the joystick-driven program selector. Re-armed on every
      load so 2nd/3rd games behave like the first. */
   SHOWLIST = 0;
   if (full_path)
   {
      autostart_mode      = 1;    /* 1 = decide auto-run vs. selector */
      autostart_countdown = 200;  /* ~4s @50fps: wait for BASIC "READY." */
   }

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

