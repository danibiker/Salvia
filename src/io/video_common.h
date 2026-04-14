#pragma once

inline void convertARGB8888ToRGB565_Fast(const uint32_t* __restrict src, int sw, int sh, std::size_t spitch,
                                         uint16_t* __restrict dst, std::size_t dpitch) {
    // Usamos punteros de fila para manejar el pitch correctamente sin multiplicaciones costosas
    for (int y = 0; y < sh; ++y) {
        const uint32_t* __restrict s = (const uint32_t*)((const uint8_t*)src + y * spitch);
        uint16_t* __restrict d = (uint16_t*)((uint8_t*)dst + y * dpitch);

        #ifdef _XBOX
        __dcbt(0, s); // Prefetch de la linea de origen (32 bits por píxel consume mucho bus)
        #endif

        int x = 0;
        // Procesamos de 2 en 2 para reducir saltos y mejorar el pipeline del PPC
        for (; x + 1 < sw; x += 2) {
            uint32_t p1 = s[x];
            uint32_t p2 = s[x + 1];

            // Conversion: RRRRRxxx GGGGGGxx BBBBBxxx -> RRRRR GGGGGG BBBBB
            d[x]     = (uint16_t)(((p1 >> 8) & 0xF800) | ((p1 >> 5) & 0x07E0) | ((p1 >> 3) & 0x001F));
            d[x + 1] = (uint16_t)(((p2 >> 8) & 0xF800) | ((p2 >> 5) & 0x07E0) | ((p2 >> 3) & 0x001F));
        }

        // Pixel restante si el ancho es impar
        if (x < sw) {
            uint32_t p = s[x];
            d[x] = (uint16_t)(((p >> 8) & 0xF800) | ((p >> 5) & 0x07E0) | ((p >> 3) & 0x001F));
        }
    }
}

inline void convert0RGB1555ToRGB565(const uint16_t* src, unsigned width, unsigned height, std::size_t pitch, uint16_t* dst, std::size_t dst_pitch) {
    for (unsigned y = 0; y < height; y++) {
        const uint16_t* src_row = (const uint16_t*)((const uint8_t*)src + y * pitch);
        uint16_t* dst_row = (uint16_t*)((uint8_t*)dst + y * dst_pitch);

        for (unsigned x = 0; x < width; x++) {
            uint16_t p = src_row[x];
            // 0RGB1555: 0 RRRRR GGGGG BBBBB
            // RGB565:   RRRRR GGGGGG BBBBB
            
            uint16_t r = (p & 0x7C00) << 1; // Mueve R de bits 10-14 a 11-15
            uint16_t g = (p & 0x03E0) << 1; // Mueve G de bits 5-9 a 6-10 (deja el bit 5 en 0)
            uint16_t b = (p & 0x001F);      // B se queda en bits 0-4
            
            // Opcional: Duplicar el bit menos significativo del verde para mejor color
            // g |= (g >> 5) & 0x0020; 

            dst_row[x] = r | g | b;
        }
    }
}

inline void convert0RGB1555ToRGB565_Fast(const uint16_t* src, unsigned width, unsigned height, std::size_t pitch, uint16_t* dst) {
    for (unsigned y = 0; y < height; y++) {
        const uint16_t* s = (const uint16_t*)((const uint8_t*)src + y * pitch);
		#ifdef _XBOX
		__dcbt(0, s); // Data Cache Block Touch (Prefetch)
		#endif
		uint16_t* d = (uint16_t*)((uint8_t*)dst + y * (width * 2));

        for (unsigned x = 0; x < width; x++) {
            uint32_t p = s[x];
            // 0RRRRRGGGGGBBBBB -> RRRRRGGGGG0BBBBB (preparando el hueco del verde)
            // En PPC, las operaciones 'and' y 'or' con constantes son muy eficientes
            d[x] = ((p & 0x7FE0) << 1) | (p & 0x001F);
        }
    }
}

inline void convert0RGB1555ToRGB565_Fast2(const uint16_t* __restrict src, unsigned width, unsigned height, std::size_t pitch, uint16_t* __restrict dst) {
    for (unsigned y = 0; y < height; y++) {
        const uint16_t* s = (const uint16_t*)((const uint8_t*)src + y * pitch);
        uint16_t* d = (uint16_t*)((uint8_t*)dst + y * (width * 2));
        
        #ifdef _XBOX
        __dcbt(0, s); // Adelanta la carga de la linea actual a la cache L1
        #endif

        unsigned x = 0;
        // Procesamos de 2 en 2 para ayudar al pipeline de la Xbox 360
        for (; x + 1 < width; x += 2) {
            uint32_t p1 = s[x];
            uint32_t p2 = s[x + 1];
            
            d[x]     = ((p1 & 0x7FE0) << 1) | (p1 & 0x001F);
            d[x + 1] = ((p2 & 0x7FE0) << 1) | (p2 & 0x001F);
        }

        // Píxel sobrante si el ancho es impar
        if (x < width) {
            uint32_t p = s[x];
            d[x] = ((p & 0x7FE0) << 1) | (p & 0x001F);
        }
    }
}

// Conversión de un píxel 0RGB1555 a RGB565
static inline uint16_t convert_pixel_0RGB1555_to_RGB565(uint16_t c)
{
    //  0RGB1555:  0 RRRRR GGGGG BBBBB
    //  RGB565:    RRRRR GGGGGG BBBBB
    //
    //  R: bits 14-10  bits 15-11  (shift left 1)
    //  G: bits  9- 5  bits 10- 5  (shift left 1, expandir de 5 a 6 bits)
    //  B: bits  4- 0  bits  4- 0  (sin cambio)

    return ((c & 0x7C00) << 1)   // R: desplaza a posición 15-11
         | ((c & 0x03E0) << 1)   // G alta: desplaza a posición 10-6
         | ((c & 0x0200) >> 4)   // G baja: duplica MSB del verde en bit 5 (expansión 5 a 6 bits)
         | ((c & 0x001F));       // B: sin cambio
}

// Conversión de un frame completo al buffer de destino
static void convert_0RGB1555_to_RGB565(
    const uint16_t* src,
    uint16_t*       dst,
    unsigned        width,
    unsigned        height,
    std::size_t     src_pitch)   // pitch en BYTES
{
    const unsigned src_stride = src_pitch / sizeof(uint16_t); // pitch en píxeles

    for (unsigned y = 0; y < height; ++y)
    {
        const uint16_t* src_row = src + y * src_stride;
        uint16_t*       dst_row = dst + y * width;

        for (unsigned x = 0; x < width; ++x)
            dst_row[x] = convert_pixel_0RGB1555_to_RGB565(src_row[x]);
    }
}

