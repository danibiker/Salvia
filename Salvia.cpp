// Salvia.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>
#include <stdio.h>
#include <cstddef> //for size_t
#include <cstdio> //fopen
#include <SDL.h>
#include "libretro.h"

// Ya no declaramos punteros a función, sino que usamos las funciones 
// que vendrán dentro del .lib (se resuelven al linkar)
extern "C" {
    void retro_init(void);
    void retro_deinit(void);
    void retro_run(void);
    void retro_get_system_info(struct retro_system_info *info);
    void retro_get_system_av_info(struct retro_system_av_info *info);
    void retro_set_environment(retro_environment_t);
    void retro_set_video_refresh(retro_video_refresh_t);
    void retro_set_audio_sample(retro_audio_sample_t);
    void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
    void retro_set_input_poll(retro_input_poll_t);
    void retro_set_input_state(retro_input_state_t);
    bool retro_load_game(const struct retro_game_info *game);
}

SDL_Surface *screen = NULL;

void retro_log_printf(enum retro_log_level level, const char *fmt, ...) {
    va_list v; va_start(v, fmt); vfprintf(stdout, fmt, v); va_end(v);
}

static bool retro_environment(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            struct retro_log_callback *log = (struct retro_log_callback*)data;
            log->log = retro_log_printf;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
            // Al devolver false, el core entiende que este frontend es simple
            // y usará la estructura retro_game_info estándar.
            return false;

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            // Opcional: Muchos cores preguntan si pueden usar RGB565 (1) o XRGB8888 (2)
            enum retro_pixel_format fmt = *(const enum retro_pixel_format *)data;
            return true; 
        }
    }
    return false; // Por defecto devolver false para comandos desconocidos
}

static void retro_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (!data) return;
    if (!screen || screen->w != width || screen->h != height) {
        screen = SDL_SetVideoMode(width, height, 16, SDL_SWSURFACE);
    }
    uint16_t *src = (uint16_t*)data;
    uint16_t *dst = (uint16_t*)screen->pixels;
    for (int y = 0; y < (int)height; y++) {
        memcpy((uint8_t*)dst + y * screen->pitch, (uint8_t*)src + y * pitch, width * 2);
    }
    SDL_Flip(screen);
}

// Stubs
static void retro_input_poll(void) {}
static int16_t retro_input_state(unsigned p, unsigned d, unsigned i, unsigned id) { return 0; }
static void retro_audio_sample(int16_t l, int16_t r) {}
static size_t retro_audio_sample_batch(const int16_t *d, size_t f) { return f; }

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s rom.md\n", argv[0]);
        return 1;
    }

    SDL_Init(SDL_INIT_VIDEO);

    retro_set_environment(retro_environment);
    retro_set_video_refresh(retro_video_refresh);
    retro_set_input_poll(retro_input_poll);
    retro_set_input_state(retro_input_state);
    retro_set_audio_sample(retro_audio_sample);
    retro_set_audio_sample_batch(retro_audio_sample_batch);

    retro_init();

	// Carga manual mínima para que el core tenga datos que procesar
	FILE *f = fopen(argv[1], "rb");
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);
	void *buffer = malloc(size);
	fread(buffer, 1, size, f);
	fclose(f);

	struct retro_game_info game = { argv[1], buffer, (size_t)size, NULL };


    // Es importante cargar la ROM antes de retro_run
    if(!retro_load_game(&game)) {
        printf("Error cargando la ROM\n");
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }
        retro_run();
    }

    retro_deinit();
    SDL_Quit();
    return 0;
}