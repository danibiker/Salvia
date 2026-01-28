#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_thread.h>

#include "image/lodepng.h"
#include <vector>
#include <string>

const Uint32 INTERVAL_SRAM_SAVE = 60000;

Uint32 lastSramSaved = 0;
void* g_sram_data_last = NULL;
std::size_t g_sram_size_last = 0;
int g_currentSlot = 0;

typedef enum {SAVE_STATE, SAVE_SRAM} ThreadAction;

extern GameMenu *gameMenu;

struct delayed_action{
	int cycles;
	int action;
	uint16_t* screenshot;
	unsigned width;
	unsigned height;

	delayed_action(){
		cycles = -1;
		action = -1;
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

void TriggerAction(ThreadAction act) {
    SDL_mutexP(g_saveQueue.saveMutex);
    g_saveQueue.action = act;
    SDL_mutexV(g_saveQueue.saveMutex);
    
    SDL_SemPost(g_saveQueue.semaphore);
}

//void TriggerSaveSram() { TriggerAction(SAVE_SRAM); }
//The savestates should be triggered in the main thread to recover the memory.
//Saving to disk can be done on the secondary thread
//void TriggerSaveState() { TriggerAction(SAVE_STATE); }

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
        gzFile fp = gzopen(sram_path, "rb");
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

void saveState(){
    SDL_mutexP(g_saveQueue.saveMutex);
    if (g_saveQueue.buffer != NULL) {
        SDL_mutexV(g_saveQueue.saveMutex);
        return;
    }
    // El mutex debe proteger la lectura de action_postponed si este se modifica en otro hilo
    // Pero lo más importante es resetear el puntero original.

    std::size_t state_size = retro_serialize_size();
    if (state_size > 0) {
        void *buffer = malloc(state_size);
        if (retro_serialize(buffer, state_size)) {
            
            g_saveQueue.buffer = buffer;
            g_saveQueue.bufferSize = state_size;
            
            // TRANSFERENCIA SEGURA:
            g_saveQueue.screenshot = action_postponed.screenshot;
            g_saveQueue.width = action_postponed.width;
            g_saveQueue.height = action_postponed.height;
            
            // IMPORTANTE: Dejamos el puntero original a NULL para que 
            // take_screenshot no haga 'delete[]' de una memoria que ahora pertenece a la cola.
            action_postponed.screenshot = NULL; 

            g_saveQueue.targetPath = getSlotPath(gameMenu->getRomPaths()->savestate, g_currentSlot);
            g_saveQueue.action = SAVE_STATE;
			g_saveQueue.slot = g_currentSlot;
            
            SDL_mutexV(g_saveQueue.saveMutex);
            SDL_SemPost(g_saveQueue.semaphore);
        } else {
            free(buffer);
            SDL_mutexV(g_saveQueue.saveMutex);
        }
    } else {
        SDL_mutexV(g_saveQueue.saveMutex);
    }
}

void loadState(){
	const std::string state_path = getSlotPath(gameMenu->getRomPaths()->savestate, g_currentSlot);

	// 1. Obtener el tamaño que espera el core
	std::size_t state_size = retro_serialize_size();
	if (state_size == 0) return;

	// 2. Abrir el archivo con zlib
	gzFile file = gzopen(state_path.c_str(), "rb");
	if (!file) {
		LOG_ERROR("No se pudo abrir el archivo: %s", state_path);
		gameMenu->showSystemMessage("No se pudo abrir el archivo: " + std::string(state_path), 3000);
		return;
	}

	// 3. Reservar memoria para los datos descomprimidos
	void* buffer = malloc(state_size);
	if (!buffer) {
		gzclose(file);
		return;
	}

	// 4. Leer y descomprimir directamente al buffer
	// gzread devuelve el número de bytes descomprimidos
	int bytesRead = gzread(file, buffer, (unsigned)state_size);
	gzclose(file);

	bool success = false;
	if (bytesRead > 0) {
		// 5. Inyectar los datos en el núcleo
		// IMPORTANTE: Esto debe ocurrir en el hilo principal (emulación)
		success = retro_unserialize(buffer, state_size);
		gameMenu->showSystemMessage("Estado cargado de slot " + Constant::TipoToStr(g_currentSlot), 3000);
	}

	free(buffer);
}

bool guardar_comprimido_zlib(const char* path, void *buffer, std::size_t buffer_size){
	// Aquí puedes escribir 'buffer' en un archivo usando fwrite
	gzFile file = gzopen(path, "wb9"); // "9" es el nivel máximo de compresión

	if (file) {
		gzwrite(file, buffer, (unsigned)buffer_size);
		gzclose(file);
		return true;
	} else {
		return false;
	}
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

// Función que ejecutará el hilo
int SaveThreadFunc(void* data) {
    SaveData* sd = (SaveData*)data;
    while (sd->running) {
        SDL_SemWait(sd->semaphore);
        if (!sd->running) break;

        // Copiamos TODOS los datos necesarios dentro del mutex
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
					bool ret = guardar_archivo_raw(localPath.c_str(), localBuffer, localSize);
					if (!ret)
						gameMenu->showSystemMessage("Error al guardar SRAM: " + localPath, 3000);

					free(localBuffer); // Ahora es seguro porque es una copia dedicada

					SDL_mutexP(sd->saveMutex);
					sd->buffer = NULL;
					sd->bufferSize = 0;
					SDL_mutexV(sd->saveMutex);
				}
				break;
			case SAVE_STATE:
				if (localBuffer) {
					guardar_comprimido_zlib(localPath.c_str(), localBuffer, localSize);
        
					if (localScreenshot) {
						// Guardamos el PNG
						GuardarCapturaPNG(localPath + STATE_IMG_EXT, (uint16_t*)localScreenshot, width, height);
            
						// LIBERACIÓN OBLIGATORIA
						delete[] (uint16_t*)localScreenshot;
					}

					free(localBuffer);

					// Limpiar la cola para permitir el siguiente guardado
					SDL_mutexP(sd->saveMutex);
					sd->buffer = NULL;
					sd->screenshot = NULL; // Indicamos que ya terminamos con el buffer de imagen
					gameMenu->showSystemMessage("Estado guardado en slot " + Constant::TipoToStr(localSlot), 3000);
					SDL_mutexV(sd->saveMutex);
				}
				break;
        }
    }
    return 0;
}

// Inicialización
void InitSaveSystem() {
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


