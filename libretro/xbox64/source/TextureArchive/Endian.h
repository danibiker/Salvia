#ifndef ENDIAN_H_
#define ENDIAN_H_

#ifndef __BIG_ENDIAN__

static inline void swap(unsigned char& data) { }

static inline void swap(unsigned short& data) {
   unsigned short swapped;
   unsigned char* d = reinterpret_cast<unsigned char*>(&data);
   unsigned char* s = reinterpret_cast<unsigned char*>(&swapped);
   s[0] = d[1], s[1] = d[0];
   data = swapped;
}

static inline void swap(unsigned int& data) {
   unsigned int swapped;
   unsigned char* d = reinterpret_cast<unsigned char*>(&data);
   unsigned char* s = reinterpret_cast<unsigned char*>(&swapped);
   s[0] = d[3], s[1] = d[2], s[2] = d[1], s[3] = d[0];
   data = swapped;
}

static inline void swap(unsigned long long& data) {
   unsigned int* d = reinterpret_cast<unsigned int*>(&data);
   /* [Windows, 2026-10-26] Ademas de voltear cada mitad hay que cambiarlas de
    * sitio: sin eso 0xF0..F7 salia 0xF4F5F6F7F0F1F2F3 y el crc64 de cada
    * textura del .pak no casaba nunca en la busqueda. */
   unsigned int t = d[0];
   d[0] = d[1];
   d[1] = t;
   swap(d[0]);
   swap(d[1]);
}

/* [Windows, logs (5)] Esta rama (little-endian) Wii64 no la compilo nunca.
 * readBinaryWithSize (ArchiveReader.cpp) tambien la llama con ARRAYS -- las
 * cadenas del .pak y el icono --, que son bytes y no se voltean: lo mismo
 * que hace la plantilla vacia de la rama big-endian con ellos. */
template <typename T, size_t N>
static inline void swap(T (&data)[N]) { (void)data; }

#else

template <typename T>
static inline void swap(T& data) { }

#endif

#endif /* ENDIAN_H_ */
