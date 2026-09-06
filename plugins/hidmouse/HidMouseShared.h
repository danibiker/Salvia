/*
    HidMouseShared.h — contrato compartido entre el plugin residente de DashLaunch
    (hidmouse.xex) y el consumidor (Salvia / libSDLx360).

    El plugin es un sysdll residente que lee el raton USB HID (hooks de kernel) y
    ACUMULA los deltas en una struct de este tipo (g_hidMouseShared). Salvia, desde
    el titulo actual, LEE esa struct de la memoria del plugin (en RGH/JTAG un titulo
    puede leer memoria de kernel) y empuja los eventos a SDL. Salvia no hookea nada.

    Como el plugin es residente (nunca se descarga), el raton sobrevive a todos los
    XLaunchNewImage: sin crash y sin replug.
*/
#ifndef HIDMOUSE_SHARED_H
#define HIDMOUSE_SHARED_H

#include <stdint.h>

#define HIDMOUSE_SHARED_MAGIC 0x484D5345u   /* 'HMSE' */

/* Version del layout BASE (el bloque agregado). Se queda en 1 A PROPOSITO.

   El consumidor localiza el canal escaneando la imagen del plugin en busca de la
   firma de dos palabras `magic` seguido de `version` (ver LocateShared en
   SDL_xboxhidmouse.cpp). Subir este numero al ampliar la struct romperia esa
   busqueda en TODOS los .xex ya compilados de Salvia: no encontrarian la firma y
   se quedarian sin raton en silencio (el fail-safe se los tragaria sin ruido).

   Por eso el bloque base es INTOCABLE -- mismos campos, mismo orden, misma
   version -- y lo nuevo va detras, en un bloque de extension con su propia firma
   (HIDMOUSE_EXT_MAGIC + HIDMOUSE_EXT_VERSION). Asi las dos direcciones de
   compatibilidad funcionan:

     - plugin viejo (solo base) + Salvia nueva: encuentra la firma base, la de
       extension no casa -> un raton, comportamiento de siempre.
     - plugin nuevo + Salvia vieja: encuentra la firma base y lee el agregado,
       ignorando lo que hay detras.

   Si algun dia hace falta un tercer bloque, se repite el patron. */
#define HIDMOUSE_SHARED_VERSION 1u

/* --- Bloque de extension: deltas POR RATON --------------------------------
   El agregado del bloque base suma todos los ratones en uno solo, que es lo que
   se quiere para el cursor del menu. Para tener dos ratones independientes (p.ej.
   dos RETRO_DEVICE_LIGHTGUN) el consumidor lee estos slots. */
#define HIDMOUSE_EXT_MAGIC    0x484D5832u   /* 'HMX2' */
#define HIDMOUSE_EXT_VERSION  2u

/* Debe coincidir con el tamano de g_mice[] en main.cpp. */
#define HIDMOUSE_MAX_DEVICES  4

typedef struct HidMouseDevice {
    /* Acumuladores CUMULATIVOS de ESTE raton, con la misma convencion que el
       agregado: el plugin solo suma y el consumidor calcula el delta por
       diferencia contra su ultima lectura. */
    volatile int32_t  accumX;
    volatile int32_t  accumY;
    volatile int32_t  accumW;
    volatile uint32_t buttons;    /* bit0=izq, bit1=der, bit2=medio (nivel actual) */
    volatile uint32_t seq;        /* reports de este raton; "señal de vida" por slot */
    volatile uint32_t connected;  /* 1 mientras el raton esta reclamado en este slot */

    /* Identidad del dispositivo (0 si no se pudo leer el descriptor). Los slots se
       asignan por "primer hueco libre", asi que un replug puede reordenarlos: sin
       identidad, desconectar el raton del slot 0 asciende al otro y el jugador 2
       pasa a ser el 1 a mitad de partida. Con vid/pid el consumidor puede fijar
       "este raton es el puerto 1" y que el orden fisico deje de importar. */
    volatile uint32_t vid;
    volatile uint32_t pid;

    volatile uint32_t hasWheel;   /* 1 si el report incluye rueda (>= 4 bytes) */
    volatile uint32_t reserved;   /* a cero; futuro */
} HidMouseDevice;

typedef struct HidMouseShared {
    /* ===== Bloque BASE (version 1). NO cambiar orden ni tipos: hay .xex ya
       compilados leyendo exactamente esto. ===== */
    volatile uint32_t magic;     /* == HIDMOUSE_SHARED_MAGIC cuando el plugin esta activo */
    volatile uint32_t version;   /* == HIDMOUSE_SHARED_VERSION */
    volatile uint32_t seq;       /* se incrementa en cada report; "señal de vida" */
    /* Acumuladores CUMULATIVOS (el plugin solo suma, nunca los resetea; envuelven
       a 2^32). El consumidor (Salvia) guarda el ultimo valor leido y calcula el
       delta = actual - ultimo. Asi el consumidor solo LEE esta struct (nunca
       escribe en la memoria del plugin) y cada .xex se "ceba" en su primera
       lectura tras un relanzamiento.
       Con varios ratones conectados esto es la SUMA de todos (y los botones, el
       OR): un unico raton virtual. Para separarlos, usar dev[] mas abajo. */
    volatile int32_t  accumX;    /* X cumulativo */
    volatile int32_t  accumY;    /* Y cumulativo */
    volatile int32_t  accumW;    /* rueda cumulativa */
    volatile uint32_t buttons;   /* bit0=izq, bit1=der, bit2=medio (nivel actual) */

    /* ===== Bloque de EXTENSION (version 2). Solo valido si extMagic y
       extVersion casan; un plugin antiguo ni siquiera tiene estos campos. ===== */
    volatile uint32_t extMagic;    /* == HIDMOUSE_EXT_MAGIC */
    volatile uint32_t extVersion;  /* == HIDMOUSE_EXT_VERSION */
    volatile uint32_t maxDevices;  /* == HIDMOUSE_MAX_DEVICES (tamano real de dev[]) */
    volatile uint32_t numDevices;  /* ratones conectados AHORA MISMO */
    HidMouseDevice    dev[HIDMOUSE_MAX_DEVICES];
} HidMouseShared;

/*
    Localizacion del canal SIN .map (no hay que ajustar ninguna direccion).

    El plugin es sysdll con base FIJA 0x81F00000 (ver xex.xml). El consumidor
    (Salvia) NO necesita la VA exacta de g_hidMouseShared: escanea la imagen del
    plugin desde su base buscando la firma de 2 palabras (magic seguido de
    version). Asi encuentra la struct este donde este dentro de la imagen, y no
    hay que tocar nada aunque se recompile el plugin y cambie el layout.

    Ver LocateShared() en SDL_xboxhidmouse.cpp (Salvia). Base y tamaño de escaneo:
*/
#define HIDMOUSE_PLUGIN_BASE  0x81F00000u
#define HIDMOUSE_SCAN_BYTES   0x00080000u   /* 512 KB (cota; SEH corta al final real) */

#endif /* HIDMOUSE_SHARED_H */
