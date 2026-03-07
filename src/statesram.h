#pragma once

#include <vector>
#include <string>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_thread.h>
#include "image/lodepng.h"
#include <utils/langmanager.h>

const Uint32 INTERVAL_SRAM_SAVE = 60000;

Uint32 lastSramSaved = 0;
void* g_sram_data_last = NULL;
std::size_t g_sram_size_last = 0;
int g_currentSlot = 0;

typedef enum {SAVE_STATE, SAVE_SRAM, SAVE_NONE} ThreadAction;

extern GameMenu *gameMenu;

struct delayed_action{
	int cycles;
	ThreadAction action;
	uint16_t* screenshot;
	unsigned width;
	unsigned height;

	delayed_action(){
		cycles = -1;
		action = SAVE_NONE;
		screenshot = NULL;
		width = 0;
		height = 0;
	}
} action_postponed;

// Estructura para compartir datos con el hilo de guardados
struct SaveData {
    SDL_Thread* thread;
    SDL_sem* semaphore;
	SDL_mutex* saveMutex;
    bool running;
	ThreadAction action;
	std::string targetPath;
	void *buffer;
	void *screenshot;
	unsigned width;
	unsigned height;
	int slot;
	std::size_t bufferSize;
} g_saveQueue;

bool GuardarCapturaPNG(const std::string& ruta, uint16_t* buffer, int w, int h) {
    if (!buffer) return false;

    // 1. Crear un vector para los datos RGB (3 bytes por píxel)
    std::vector<unsigned char> rgb_buffer;
    rgb_buffer.resize(w * h * 3);

    // 2. Convertir de RGB565 a RGB888
    for (int i = 0; i < w * h; ++i) {
        uint16_t pixel = buffer[i];
        
        // Extraer componentes y expandir de 5/6 bits a 8 bits
        // Usamos desplazamiento y bitwise OR para mantener el brillo correcto
        uint8_t r = ((pixel >> 11) & 0x1F);
        uint8_t g = ((pixel >> 5) & 0x3F);
        uint8_t b = (pixel & 0x1F);

        rgb_buffer[i * 3 + 0] = (r << 3) | (r >> 2);
        rgb_buffer[i * 3 + 1] = (g << 2) | (g >> 4);
        rgb_buffer[i * 3 + 2] = (b << 3) | (b >> 2);
    }

    // 3. Codificar y guardar el archivo en el HDD/USB de la Xbox
    // lodepng::encode devuelve 0 si tiene éxito
    unsigned error = lodepng::encode(ruta, rgb_buffer, w, h, LCT_RGB, 8);

    if (error) {
        // En caso de error, puedes depurar con: lodepng_error_text(error)
        return false;
    }

    return true;
}

bool guardar_comprimido_zlib(const char* path, void *buffer, std::size_t buffer_size){
	// Aquí puedes escribir 'buffer' en un archivo usando fwrite
	gzFile file = gzopen(path, "wb9"); // "9" es el nivel máximo de compresión

	if (file) {
		LOG_DEBUG("gzwrite: %s bytes; %d\n", path, buffer_size);
		gzwrite(file, buffer, (unsigned)buffer_size);
		gzclose(file);
		return true;
	} else {
		LOG_DEBUG("gzwrite error al escribir: %s bytes; %d; causa: %s\n", path, buffer_size, strerror(errno));
		return false;
	}
}

std::string getSlotPath(const std::string& baseStatePath, int slot) {
    if (slot == 0) return baseStatePath; // slot 0 -> "juego.state"
    
    char extension[10];
    sprintf(extension, "%d", slot); // convierte slot a string
    return baseStatePath + extension; // ej: "juego.state1"
}

void loadSram(const char* sram_path) {
    std::size_t size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void* data = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);

    if (size > 0 && data) {
		std::string path = Constant::checkPath(sram_path);
		gzFile fp = gzopen(path.c_str(), "rb");
        if (fp) {
			gzread(fp, data, size);
            gzclose(fp);
        }
    }
}

void saveSram(const char* sram_path) {
    std::size_t size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void* data = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);

    if (size == 0 || !data) return;

    // 1. Comparación con nuestra copia persistente
    bool needsSave = (g_sram_data_last == NULL) || 
                     (g_sram_size_last != size) || 
                     (memcmp(g_sram_data_last, data, size) != 0);

    if (needsSave) {
        // 2. Actualizamos nuestra copia de comparación
        if (g_sram_size_last != size) {
            if (g_sram_data_last) free(g_sram_data_last);
            g_sram_data_last = malloc(size);
            g_sram_size_last = size;
        }
        if (g_sram_data_last) memcpy(g_sram_data_last, data, size);

        // 3. CREAMOS UNA COPIA NUEVA PARA EL HILO (Para que el hilo pueda hacer free con seguridad)
        void* hiloBuffer = malloc(size);
        if (hiloBuffer) {
            memcpy(hiloBuffer, data, size);

            SDL_mutexP(g_saveQueue.saveMutex);
            // Si el hilo aún no ha terminado el anterior, liberamos para evitar leak
            if (g_saveQueue.buffer) free(g_saveQueue.buffer); 
            
            g_saveQueue.buffer = hiloBuffer;
            g_saveQueue.bufferSize = size;
            g_saveQueue.targetPath = sram_path;
            g_saveQueue.action = SAVE_SRAM;
            SDL_mutexV(g_saveQueue.saveMutex);

            SDL_SemPost(g_saveQueue.semaphore);
        }
    }
}

void saveState() {
    // 1. Obtener tamaños de ambos estados
    std::size_t core_state_size = retro_serialize_size();
    std::size_t ra_state_size = 0;
    
    rc_client_t* ra_client = Achievements::instance()->getClient();
    if (ra_client) {
        ra_state_size = rc_client_progress_size(ra_client);
    }

    // 2. Calcular tamaño total del buffer: Core + Marcador(4) + TamañoRA(4) + DatosRA
    // Reservamos espacio extra para una cabecera de seguridad de RA
    std::size_t total_buffer_size = core_state_size + 8 + ra_state_size;
    void *buffer = malloc(total_buffer_size);
    
    if (!buffer) return;

    // 3. Serializar el Core de Libretro al principio del buffer
    if (retro_serialize(buffer, core_state_size)) {
        
        // 4. Serializar datos de RetroAchievements a continuación
        uint8_t* ra_ptr = (uint8_t*)buffer + core_state_size;
        
        // Escribimos un marcador "RCHV" y el tamaño para poder leerlo luego de forma segura
        std::memcpy(ra_ptr, "RCHV", 4);
        std::memcpy(ra_ptr + 4, &ra_state_size, 4);
        
        if (ra_state_size > 0 && ra_client) {
            rc_client_serialize_progress(ra_client, ra_ptr + 8);
        }

        // 5. Preparar la transferencia a la cola del hilo de guardado (I/O thread)
        std::string targetPath = getSlotPath(gameMenu->getRomPaths()->savestate, g_currentSlot);
        void* hiloBuffer = malloc(total_buffer_size);
        
        if (hiloBuffer) {
            std::memcpy(hiloBuffer, buffer, total_buffer_size);

            SDL_mutexP(g_saveQueue.saveMutex);
            
            if (g_saveQueue.buffer) {
                free(g_saveQueue.buffer);
            }
            
            g_saveQueue.buffer = hiloBuffer;
            g_saveQueue.bufferSize = total_buffer_size;
            g_saveQueue.targetPath = targetPath;
            g_saveQueue.slot = g_currentSlot;

            // Gestión de la captura de pantalla para el Slot
            g_saveQueue.width = action_postponed.width;
            g_saveQueue.height = action_postponed.height;
            const std::size_t total_pixels = g_saveQueue.width * g_saveQueue.height;

            if (total_pixels > 0 && action_postponed.screenshot != NULL) {
                g_saveQueue.screenshot = new uint16_t[total_pixels];
                std::memcpy(g_saveQueue.screenshot, action_postponed.screenshot, total_pixels * sizeof(uint16_t));
            }
            
            g_saveQueue.action = SAVE_STATE;

            SDL_mutexV(g_saveQueue.saveMutex);
            SDL_SemPost(g_saveQueue.semaphore);
        }
    } 

    // Limpieza de memoria temporal del hilo principal
    free(buffer);
    
    if (action_postponed.screenshot) {
        delete[] action_postponed.screenshot;
        action_postponed.screenshot = NULL;
    }
}

void loadState(){
	cfg::t_cfg_props* cfg = gameMenu->getCfgLoader()->configMain;

	if (cfg[cfg::enableAchievements].valueBool && cfg[cfg::hardcoreRA].valueBool){
		gameMenu->showLangSystemMessage("msg.error.hardcore.loadstate", 3000);
		return;
	}

    const std::string state_path = Constant::checkPath(getSlotPath(gameMenu->getRomPaths()->savestate, g_currentSlot));
    // 1. Obtener el tamaño que espera el núcleo (Core)
    std::size_t core_state_size = retro_serialize_size();
    if (core_state_size == 0) return;

    // 2. Abrir el archivo comprimido con zlib
    gzFile file = gzopen(state_path.c_str(), "rb");
    if (!file) {
        LOG_ERROR("No se pudo abrir el archivo: %s", state_path.c_str());
        gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.error.fileopen") + std::string(state_path), 3000);
        return;
    }

    // 3. Cargar el estado del Núcleo (Core)
    void* core_buffer = malloc(core_state_size);
    if (!core_buffer) {
        gzclose(file);
        return;
    }

    // Leemos exactamente el tamaño que el core espera
    int bytesRead = gzread(file, core_buffer, (unsigned)core_state_size);
    
    if (bytesRead == (int)core_state_size) {
        // Inyectamos los datos en el núcleo de emulación
        retro_unserialize(core_buffer, core_state_size);
        
        // 4. Intentar cargar el bloque de RetroAchievements (Cabecera de 8 bytes)
        char ra_marker[4];
        uint32_t ra_data_size = 0;
        
		uint8_t* ra_buffer = NULL;
        // Intentamos leer el marcador "RCHV" y el tamaño de los datos
        if (gzread(file, ra_marker, 4) == 4 && memcmp(ra_marker, "RCHV", 4) == 0) {
            if (gzread(file, &ra_data_size, 4) == 4 && ra_data_size > 0) {
                ra_buffer = (uint8_t*)malloc(ra_data_size);
                if (ra_buffer && gzread(file, ra_buffer, (unsigned)ra_data_size) != (int)ra_data_size) {
					free(ra_buffer);
                    ra_buffer = NULL;
                }
            }
        } 

		rc_client_t* ra_client = Achievements::instance()->getClient();
        if (ra_client) {
			//When loading a save state that does not have runtime state information, 
			//rc_client_deserialize_progress should be called with NULL to reset the runtime state.
            rc_client_deserialize_progress(ra_client, ra_buffer);
        }

		if (ra_buffer)
			free(ra_buffer);

        gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.state.load") + Constant::TipoToStr(g_currentSlot), 3000);
    } else {
        LOG_ERROR("Error de lectura: El archivo es más pequeño de lo esperado.");
    }

    // Limpieza final
    free(core_buffer);
    gzclose(file);
}

bool guardar_archivo_raw(const char* path, void* buffer, std::size_t size) {
    FILE* fp = fopen(path, "wb");
    if (fp) {
        fwrite(buffer, 1, size, fp);
        fclose(fp);
        return true;
    } else {
        return false;
    }
}

// Función que ejecutará el hilo
int SaveThreadFunc(void* data) {
    SaveData* sd = (SaveData*)data;
    while (sd->running) {
        SDL_SemWait(sd->semaphore);
        if (!sd->running) break;

        // Copiamos TODOS los datos necesarios dentro del mutex
		dirutil dir;
        SDL_mutexP(sd->saveMutex);
        ThreadAction actionActual = sd->action;
        std::string localPath = sd->targetPath; // Copia local segura
        void* localBuffer = sd->buffer;
        std::size_t localSize = sd->bufferSize;
		void* localScreenshot = sd->screenshot;
		unsigned width = sd->width;
		unsigned height = sd->height;
		int localSlot = sd->slot;
        SDL_mutexV(sd->saveMutex);

        switch (actionActual) {
			case SAVE_SRAM:
				if (localBuffer) {
					bool ret = guardar_archivo_raw(Constant::checkPath(localPath).c_str(), localBuffer, localSize);
					if (!ret)
						gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.error.sram") + localPath, 3000);

					free(localBuffer); // Ahora es seguro porque es una copia dedicada

					SDL_mutexP(sd->saveMutex);
					sd->buffer = NULL;
					sd->bufferSize = 0;
					sd->action = SAVE_NONE; // IMPORTANTE: Resetea la acción
					SDL_mutexV(sd->saveMutex);
				}
				break;
			case SAVE_STATE:

				if (localBuffer) {
					std::string statePath = Constant::checkPath(localPath);
					if (guardar_comprimido_zlib(statePath.c_str(), localBuffer, localSize)){
						if (localScreenshot != NULL) {
							std::string imgPath = Constant::checkPath(localPath + STATE_IMG_EXT);
							GuardarCapturaPNG(imgPath, (uint16_t*)localScreenshot, width,  height);
							gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.state.save") + Constant::TipoToStr(localSlot), 3000);
						} else {
							gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.error.savestate.image") + localPath + STATE_IMG_EXT, 3000);
						}
					} else {
						gameMenu->showSystemMessage(LanguageManager::instance()->get("msg.error.savestate") + std::string(strerror(errno)) + "; " + localPath, 3000);
					}

					free(localBuffer); // Ahora es seguro porque es una copia dedicada
					delete[] (uint16_t*)localScreenshot; // Liberar memoria de imagen

					SDL_mutexP(sd->saveMutex);
					sd->buffer = NULL;
					sd->bufferSize = 0;
					sd->action = SAVE_NONE; // IMPORTANTE: Resetea la acción
					SDL_mutexV(sd->saveMutex);
				}
				break;
        }
    }
    return 0;
}

// Inicialización
void initSaveSystem() {
    g_saveQueue.running = true;
    g_saveQueue.semaphore = SDL_CreateSemaphore(0);
    g_saveQueue.thread = SDL_CreateThread(SaveThreadFunc, &g_saveQueue);
	
	// Crear el mutex
    g_saveQueue.saveMutex = SDL_CreateMutex();

    if (g_saveQueue.saveMutex == NULL) {
        // Manejar error: No se pudo crear el mutex
        return;
    }
}

void deinitSaveSystem() {
    g_saveQueue.running = false;
    SDL_SemPost(g_saveQueue.semaphore); // Despertar hilo
    
    if (g_saveQueue.thread) {
        SDL_WaitThread(g_saveQueue.thread, NULL); // Esperar a que termine de escribir
        g_saveQueue.thread = NULL;
    }
    
    if (g_saveQueue.saveMutex) {
        SDL_DestroyMutex(g_saveQueue.saveMutex);
        g_saveQueue.saveMutex = NULL;
    }

	// 3. Liberamos los recursos de SDL
    if (g_saveQueue.semaphore) SDL_DestroySemaphore(g_saveQueue.semaphore);
    
    // 4. Liberamos la memoria del buffer de SRAM
    if (g_sram_data_last) {
        free(g_sram_data_last);
        g_sram_data_last = NULL;
    }
}
