/**
 * glN64_GX - CRC.cpp
 * Copyright (C) 2003 Orkin
 * Copyright (C) 2008, 2009 sepp256 (Port to Wii/Gamecube/PS3)
 *
 * glN64 homepage: http://gln64.emulation64.com
 * Wii64 homepage: http://www.emulatemii.com
 * email address: sepp256@gmail.com
 *
**/

#ifndef __LINUX__
# include <windows.h>
#else
# include "../main/winlnxdefs.h"
#endif // __LINUX__
#define XXH_PRIVATE_API
#define XXH_FORCE_MEMORY_ACCESS 2
#define XXH_FORCE_NATIVE_FORMAT 1
#define XXH_FORCE_ALIGN_CHECK 0
#include "../main/xxhash.h"

#define CRC32_POLYNOMIAL     0x04C11DB7

unsigned long CRCTable[ 256 * 4 ];

DWORD Reflect( DWORD ref, char ch )
{
     DWORD value = 0;

     // Swap bit 0 for bit 7
     // bit 1 for bit 6, etc.
     for (int i = 1; i < (ch + 1); i++)
     {
          if(ref & 1)
               value |= 1 << (ch - i);
          ref >>= 1;
     }
     return value;
}

void CRC_BuildTable()
{
    DWORD crc;

    for (int i = 0; i < 256; i++)
	{
        crc = Reflect( i, 8 ) << 24;
        for (int j = 0; j < 8; j++)
			crc = (crc << 1) ^ (crc & (1 << 31) ? CRC32_POLYNOMIAL : 0);
        
        CRCTable[i] = Reflect( crc, 32 );
    }

    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < 3; j++)
            CRCTable[256 * (j + 1) + i] = (CRCTable[256 * j + i] >> 8) ^ CRCTable[CRCTable[256 * j + i] & 0xFF];
}
}

DWORD CRC_Calculate( DWORD crc, void *buffer, DWORD count )
{
    BYTE *p;
	DWORD orig = crc;

    p = (BYTE*) buffer;

    while (count > 3)
    {
        crc ^= *(unsigned int*) p; p += 4;
        crc = CRCTable[3 * 256 + (crc & 0xFF)]
            ^ CRCTable[2 * 256 + ((crc >> 8) & 0xFF)]
            ^ CRCTable[1 * 256 + ((crc >> 16) & 0xFF)]
            ^ CRCTable[0 * 256 + ((crc >> 24))];

        count -= 4;
    }

	while (count--) 
		crc = (crc >> 8) ^ CRCTable[(crc & 0xFF) ^ *p++];

    return crc ^ orig;
}

DWORD CRC_CalculatePalette( DWORD crc, void *buffer, DWORD count )
{
    BYTE *p;
	DWORD orig = crc;

    p = (BYTE*) buffer;
    while (count--)
	{
		crc = (crc >> 8) ^ CRCTable[(crc & 0xFF) ^ *p++];
		crc = (crc >> 8) ^ CRCTable[(crc & 0xFF) ^ *p++];

		p += 6;
	}

    return crc ^ orig;
}

/* [Xbox360] ★★ GLN64_HASH -- QUE HASH USA LA CACHE DE TEXTURAS.
 *
 * EL PROBLEMA, YA DIAGNOSTICADO CON DATO.  `[GLN64H]` mide **420 MB/s**
 * hasheando bloques grandes y alineados = **7,4 ciclos por byte**.  No es el
 * coste por llamada (`porfila=0`, 818 B/llamada), no es byte-swapping
 * (`XXH_FORCE_NATIVE_FORMAT 1`), no es un build sin optimizar, y no es
 * `_rotl` (probado en el log 39: 421 -> 420 MB/s, cero).
 *
 * Son las MULTIPLICACIONES.  El bucle interno de XXH32 hace 8 multiplicaciones
 * enteras por cada 16 bytes, y en el PPE `mullw` es microcodificada y NO
 * encauzada: ~12-14 ciclos con parada de emision.  8 x 13 / 16 = 6,5
 * ciclos/byte, mas las cargas.  Eso ES la medida.
 *
 * O sea que XXH32 no esta mal configurado: es multiplicativo por diseno y
 * esa es la peor instruccion de este nucleo.  Hace falta un hash SIN
 * multiplicaciones.
 *
 *   0  XXH32            lo de hoy.  8 mul / 16 bytes.
 *   1  Adler-32         2 sumas por byte, CERO multiplicaciones.
 *   2  CRC32 slice-by-4 1 consulta a tabla por byte, CERO multiplicaciones.
 *
 * ★ EL 2 ES EL BUENO, Y ADEMAS YA ESTABA AQUI.  `CRC_Calculate` (arriba, en
 * este mismo fichero) es el hash ORIGINAL de glN64: slice-by-4 sobre una
 * tabla de 4 KB que se queda en L1, sin una sola multiplicacion.  Es codigo
 * muerto desde que alguien lo cambio por XXH32 -- `CRC_BuildTable()` no lo
 * llama nadie en todo el arbol.  No hay nada que inventar.
 *
 * ★★ Y POR QUE NO ADLER, aunque sea el mas rapido de los tres.  El hash es
 * la CLAVE DE LA CACHE DE TEXTURAS: una colision no es una estadistica, es
 * UNA TEXTURA EQUIVOCADA en pantalla.  `TextureCache_Update` compara ademas
 * 12 campos (tamanyo, formato, clamp...), pero justo NO filtra el caso
 * peligroso: dos texturas distintas del MISMO tamanyo y formato, donde el
 * hash es lo unico que las separa.  Adler-32 se porta mal precisamente con
 * bloques cortos y parecidos, que es lo que son las texturas de N64.  Esta
 * como opcion 1 para MEDIR el techo sin multiplicaciones, no para quedarse.
 *
 * CRC32 da esa misma velocidad con una distribucion buena de verdad.
 *
 * ★ No se reutiliza `CRC_Calculate` tal cual por su `return crc ^ orig`:
 * CRC32 es lineal sobre GF(2), asi que ese XOR final con la semilla mete un
 * mapa `L+I` que puede ser singular -- dos semillas distintas dando el mismo
 * resultado con los mismos datos.  Para un hash encadenado (texels y luego
 * paleta) eso es exactamente la colision que no queremos. */
#define GLN64_HASH 2

#if GLN64_HASH == 2
/* La tabla se construye una sola vez, a la primera.  Hace falta porque
 * `CRC_BuildTable()` no lo llama nadie: `CRCTable` esta a cero. */
static int s_crcTablaLista = 0;

static DWORD crc32_bloque( DWORD crc, const unsigned char *p, DWORD count )
{
    if (!s_crcTablaLista) { CRC_BuildTable(); s_crcTablaLista = 1; }

    while (count > 3)
    {
        crc ^= *(const unsigned int*)p; p += 4;
        crc = CRCTable[3 * 256 + ( crc        & 0xFF)]
            ^ CRCTable[2 * 256 + ((crc >>  8) & 0xFF)]
            ^ CRCTable[1 * 256 + ((crc >> 16) & 0xFF)]
            ^ CRCTable[0 * 256 + ( crc >> 24        )];
        count -= 4;
    }
    while (count--)
        crc = (crc >> 8) ^ CRCTable[(crc & 0xFF) ^ *p++];

    return crc;
}
#endif /* GLN64_HASH == 2 */

#if GLN64_HASH == 1

#define ADLER_BASE 65521UL   /* el primo mas grande por debajo de 65536 */
#define ADLER_NMAX 5552      /* mayor n con 255n(n+1)/2 + (n+1)(BASE-1) < 2^32 */

#define ADLER_DO1(buf,i)  { s1 += (buf)[i]; s2 += s1; }
#define ADLER_DO2(buf,i)  ADLER_DO1(buf,i); ADLER_DO1(buf,i+1);
#define ADLER_DO4(buf,i)  ADLER_DO2(buf,i); ADLER_DO2(buf,i+2);
#define ADLER_DO8(buf,i)  ADLER_DO4(buf,i); ADLER_DO4(buf,i+4);
#define ADLER_DO16(buf)   ADLER_DO8(buf,0); ADLER_DO8(buf,8);

static DWORD adler32_bloque( DWORD adler, const unsigned char *buf, DWORD len )
{
    unsigned long s1 = adler & 0xFFFF;
    unsigned long s2 = (adler >> 16) & 0xFFFF;

    while (len > 0)
    {
        int k = (len < ADLER_NMAX) ? (int)len : ADLER_NMAX;
        len -= (DWORD)k;
        while (k >= 16) { ADLER_DO16(buf); buf += 16; k -= 16; }
        while (k--)     { s1 += *buf++; s2 += s1; }
        s1 %= ADLER_BASE;
        s2 %= ADLER_BASE;
    }
    return (DWORD)((s2 << 16) | s1);
}
#endif /* GLN64_HASH == 1 */

DWORD Hash_Calculate( DWORD hash, void *buffer, DWORD count )
{
#if   GLN64_HASH == 2
    return crc32_bloque(hash, (const unsigned char*)buffer, count);
#elif GLN64_HASH == 1
    return adler32_bloque(hash, (const unsigned char*)buffer, count);
#else
    return XXH32(buffer, count, hash);
#endif
}
