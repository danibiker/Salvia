/** 
* This file is a stub to be able to run the frontend without 
* a statically compiled libretro core. We need it to generate 
* the default.xex used to launch all other cores
*/

#ifdef LIBRETRO_NOLIB

#include <libretro\libretro.h>

void retro_get_system_info(struct retro_system_info *info){
	info->library_name = "Salvia";
	info->library_version = "v1.0.6";
	info->valid_extensions = "";
	info->block_extract = false;
	info->need_fullpath = true;
}

void retro_run(void){};
void retro_set_environment(retro_environment_t cb){};
void retro_set_video_refresh(retro_video_refresh_t cb) {};
bool retro_load_game(const struct retro_game_info *info){return false;}
size_t retro_serialize_size(void) {return 0;}
bool retro_serialize(void *data, size_t size){return false;}
void retro_set_input_poll(retro_input_poll_t cb) {};
void retro_init(void){};
void retro_deinit(void){};
void retro_set_input_state(retro_input_state_t cb) {};
void retro_unload_game(void){};
void retro_set_audio_sample(retro_audio_sample_t cb) {};
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) {};
bool retro_unserialize(const void *data, size_t size){return false;}
void retro_cheat_reset(void){};
void retro_cheat_set(unsigned index, bool enabled, const char *code){};
size_t retro_get_memory_size(unsigned id){return 0;}
void *retro_get_memory_data(unsigned id){return NULL;}
void retro_get_system_av_info(struct retro_system_av_info *info){};
void retro_set_controller_port_device(unsigned port, unsigned device){};

#endif