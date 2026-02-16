#include <SDL.h>
#include "audiobuffer.h"

AudioBuffer::AudioBuffer(std::size_t size) : capacity(size) {
    buffer.resize(size);
	head = 0;
	tail = 0;
}

void AudioBuffer::Write(const int16_t* samples, std::size_t count) {
    for (std::size_t i = 0; i < count; i++) {
        buffer[head] = samples[i];
        head = (head + 1) % capacity;
    }
}

void AudioBuffer::Read(int16_t* stream, std::size_t count) {
    for (std::size_t i = 0; i < count; i++) {
        if (head != tail) {
            stream[i] = buffer[tail];
            tail = (tail + 1) % capacity;
        } else {
            stream[i] = 0; // Silencio si el buffer está vacío
        }
    }
}

// Añade esto a la clase AudioBuffer anterior
void AudioBuffer::WriteBlocking(const int16_t* samples, std::size_t count) {
	std::size_t free_space = 0;
    
	// Bucle de espera: si no hay espacio, dormimos el hilo un poco
	while (true) {
		// Calcular espacio disponible en el buffer circular
		if (head >= tail) free_space = capacity - (head - tail);
		else free_space = tail - head;

		if (free_space > count) break; // Hay sitio
        
		SDL_Delay(1); // Esperar 1ms y reintentar
	}

	// Una vez hay espacio, escribimos las muestras
	for (std::size_t i = 0; i < count; i++) {
		buffer[head] = samples[i];
		head = (head + 1) % capacity;
	}
}