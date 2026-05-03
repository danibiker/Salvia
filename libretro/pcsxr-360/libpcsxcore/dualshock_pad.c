/* DualShock state machine — port de Pokopom (KrossX, GPLv3).
 *
 * Implementacion del SIO state machine real del SCPH-1200 (DualShock).
 * Soporta poll normal (0x42), config mode (0x43), set mode (0x44),
 * queries (0x45/0x46/0x47/0x4C) y set rumble mapping (0x4D).
 *
 * Diferencias con el original Pokopom:
 *  - Sin C++ classes / herencia: state machine en C plano, state es
 *    array global por puerto.
 *  - Sin namespace emupro::pad::DataS: usa estructuras propias (`ds_input_t`).
 *  - Sin clases DualShock2 / Guitar / MultiTap — solo DualShock SCPH-1200.
 *    Aceptable para PSX (PS2 quirks no aplican).
 *  - Rumble se entrega via callback registrado en ds_init en lugar de
 *    Input::DualshockRumble().  El caller decide como hablar con el HW.
 *  - Cmd0() Pokopom hacia toggle analog/digital con un boton fisico del
 *    mando real; en PSXInput.cpp no tenemos ese boton (los Xbox no
 *    tienen ANALOG button), asi que se omite — el padID lo fija el
 *    frontend via ds_force_analog().
 *
 * Header: dualshock_pad.h
 */

#include "dualshock_pad.h"

#include <string.h>   /* memset, memcpy */

/* =========================================================================
 *  Constantes (de Pokopom Codes_IDs.h)
 * ========================================================================= */

#define ID_DIGITAL       0x41
#define ID_ANALOG_GREEN  0x53
#define ID_ANALOG_RED    0x73
#define ID_CONFIG        0xF3

/* Respuestas del DualShock real (SCPH-1200) para queries en config mode.
 * Indexadas para los comandos 0x46 / 0x4C que tienen variantes 0x00/0x01. */
static const uint8_t DUALSHOCK_MODEL[6] = { 0x02, 0x02, 0x00, 0x02, 0x00, 0x00 };

/* DUALSHOCK_ID[N][5]: respuestas de cmd 0x46/0x47/0x4C en bytes 4..8
 * (los primeros 4 bytes 0xF3, 0x5A, X, Y son comunes).
 *
 * Pokopom usa DUALSHOCK2_ID por defecto.  Mantenemos eso. */
static const uint8_t DUALSHOCK2_ID[5][5] = {
    { 0x00, 0x01, 0x02, 0x00, 0x0A },  /* 0x46 subcmd 0x00 */
    { 0x00, 0x01, 0x01, 0x01, 0x14 },  /* 0x46 subcmd 0x01 */
    { 0x00, 0x02, 0x00, 0x01, 0x00 },  /* 0x47 */
    { 0x00, 0x00, 0x04, 0x00, 0x00 },  /* 0x4C subcmd 0x00 */
    { 0x00, 0x00, 0x07, 0x00, 0x00 }   /* 0x4C subcmd 0x01 */
};

/* DUALSHOCK2_MODEL para cmd 0x45 (query model).  Bytes 3..8.
 * El byte 5 (LED status) lo sobreescribimos en runtime con el padID actual. */
static const uint8_t DUALSHOCK2_MODEL[6] = { 0x03, 0x02, 0x00, 0x02, 0x01, 0x00 };

/* WTF: respuesta peculiar para cmd 0x40 (reservada).  No se sabe que
 * juego la usa, pero Pokopom la incluye y mantenemos. */
static const uint8_t WTF[6] = { 0x00, 0x00, 0x02, 0x00, 0x00, 0x5A };

/* =========================================================================
 *  State per-port
 * ========================================================================= */

#define DS_NUM_PORTS  2
#define DS_BUF_SIZE   9   /* 9 bytes en intercambio SIO normal de DualShock */

typedef struct {
    /* Buffers de SIO. dataBuffer = bytes que respondemos al juego.
     * cmdBuffer = bytes que el juego nos escribio durante este intercambio. */
    uint8_t dataBuffer[DS_BUF_SIZE];
    uint8_t cmdBuffer[DS_BUF_SIZE];

    /* Identidad del pad.  0x41 = digital, 0x73 = analog (rojo). */
    uint8_t padID;

    /* Flags de modo. */
    int bConfig;      /* dentro de config mode (entre 0x43 0x01 y 0x43 0x00). */
    int bModeLock;    /* el juego pidio lock con 0x44 — el usuario no puede togglear. */

    /* Mapping de motor: si el juego mapeo small/big a un slot, contiene
     * el indice (0..5).  0xFF = sin mapeo. */
    uint8_t motorMapS;
    uint8_t motorMapL;

    /* Snapshot del input del usuario para este intercambio.  Se actualiza
     * desde ds_set_input antes del primer ds_command(counter=0). */
    ds_input_t input;
} ds_state_t;

static ds_state_t s_state[DS_NUM_PORTS];
static ds_rumble_cb_t s_rumble_cb = NULL;

/* =========================================================================
 *  Public API
 * ========================================================================= */

void ds_init(ds_rumble_cb_t rumble_cb)
{
    int p;
    s_rumble_cb = rumble_cb;
    for (p = 0; p < DS_NUM_PORTS; p++) {
        ds_reset(p);
    }
}

void ds_reset(int port)
{
    ds_state_t *s;
    if (port < 0 || port >= DS_NUM_PORTS) return;
    s = &s_state[port];

    memset(s->dataBuffer, 0xFF, DS_BUF_SIZE);
    memset(s->cmdBuffer,  0x00, DS_BUF_SIZE);
    s->dataBuffer[2] = 0x5A;

    /* Default: arrancamos en analog (red).  El juego puede togglear con
     * 0x44 si quiere digital.  PSXInput.cpp como cliente fuerza analog. */
    s->padID     = ID_ANALOG_RED;
    s->bConfig   = 0;
    s->bModeLock = 0;

    s->motorMapS = 0xFF;
    s->motorMapL = 0xFF;

    memset(&s->input, 0x00, sizeof(s->input));
    s->input.buttons = 0xFFFF;   /* sin botones pulsados */
    s->input.leftX = s->input.leftY = 0x80;
    s->input.rightX = s->input.rightY = 0x80;
}

void ds_set_input(int port, const ds_input_t *input)
{
    if (port < 0 || port >= DS_NUM_PORTS || !input) return;
    s_state[port].input = *input;
}

void ds_force_analog(int port)
{
    if (port < 0 || port >= DS_NUM_PORTS) return;
    s_state[port].padID     = ID_ANALOG_RED;
    s_state[port].bModeLock = 1;   /* lock para que toggles no lo cambien */
}

/* =========================================================================
 *  Helpers internos del state machine
 * ========================================================================= */

/* Llena dataBuffer[3..8] con button + analog data del usuario.
 * Equivalente a Pokopom DualShock::ReadInput. */
static void ds_read_input_into_data(ds_state_t *s)
{
    uint8_t *buffer = s->dataBuffer;

    if (s->padID == ID_DIGITAL) {
        /* Digital: solo botones, sin sticks.  buttonsStick masks L3/R3
         * que no existen en pad digital — pero el bitfield buttons del
         * caller ya viene con esos bits a 1 (no pulsado), asi que basta
         * con copiar.  El plugin antiguo hacia exactamente eso. */
        buffer[3] = s->input.buttons & 0xFF;
        buffer[4] = (s->input.buttons >> 8) & 0xFF;
        memset(&buffer[5], 0xFF, 4);   /* analog bytes = 0xFF (sin presencia) */
    } else {
        /* Analog (DualShock): botones + 4 ejes. */
        buffer[3] = s->input.buttons & 0xFF;
        buffer[4] = (s->input.buttons >> 8) & 0xFF;
        buffer[5] = s->input.rightX;
        buffer[6] = s->input.rightY;
        buffer[7] = s->input.leftX;
        buffer[8] = s->input.leftY;
    }
}

/* Cmd1: prepara dataBuffer[3..8] segun el comando que el juego envio.
 * Equivalente a Pokopom DualShock::Cmd1. */
static void ds_cmd1(ds_state_t *s, uint8_t cmd)
{
    uint8_t *db = s->dataBuffer;

    switch (cmd) {
        case 0x40:
            if (s->bConfig) memcpy(&db[3], WTF, 6);
            /* fallthrough */
        case 0x41:
            break;

        case 0x42:  /* Read Data + Vibrate */
            if (s->bConfig)
                memset(&db[3], 0xFF, 6);   /* en config, bytes son neutrales */
            else
                ds_read_input_into_data(s);  /* button + analog data */
            break;

        case 0x43:  /* Toggle config mode */
            if (s->bConfig)
                memset(&db[3], 0x00, 6);   /* saliendo de config */
            else
                ds_read_input_into_data(s);  /* entrando: respuesta tipo poll */
            break;

        case 0x44:  /* Set mode and lock */
            memset(&db[3], 0x00, 6);
            break;

        case 0x45:  /* Query model */
            if (s->bConfig) {
                memcpy(&db[3], DUALSHOCK2_MODEL, 6);
                db[5] = (s->padID == ID_DIGITAL) ? 0x00 : 0x01;  /* LED status */
            }
            break;

        case 0x46:
        case 0x47:
            if (s->bConfig) db[3] = 0x00;
            break;

        case 0x4C:
            if (s->bConfig) db[3] = 0x00;
            break;

        case 0x4D:  /* Set rumble mapping — devolvemos mapping previo */
            if (s->bConfig) {
                db[3] = s->motorMapS;
                db[4] = s->motorMapL;
                memset(&db[5], 0xFF, 4);
            }
            break;

        default:
            break;
    }
}

/* Cmd4: bytes 3..4 ya recibidos.  Procesa cambios de state segun cmd
 * (e.g. entrar/salir config, toggle analog).  Equivalente a Pokopom
 * DualShock::Cmd4. */
static void ds_cmd4(ds_state_t *s, uint8_t cmd)
{
    uint8_t *db = s->dataBuffer;
    uint8_t *cb = s->cmdBuffer;

    switch (cmd) {
        case 0x43:  /* Config mode toggle, byte 3 = 0/1 */
            s->bConfig = (cb[3] == 0x01) ? 1 : 0;
            break;

        case 0x44:  /* Set mode (digital / analog) + optional lock */
            if (s->bConfig) {
                s->padID = (cb[3] == 0x01) ? ID_ANALOG_RED : ID_DIGITAL;
                s->bModeLock = (cb[4] == 0x03);   /* Disgaea sends 0x01 (no lock) */
            }
            break;

        case 0x46:  /* Unknown query, byte 3 = 0/1 selecciona variante */
            if (s->bConfig) {
                if (cb[3] == 0x00) memcpy(&db[4], DUALSHOCK2_ID[0], 5);
                else                memcpy(&db[4], DUALSHOCK2_ID[1], 5);
            }
            break;

        case 0x47:
            if (s->bConfig) memcpy(&db[4], DUALSHOCK2_ID[2], 5);
            break;

        case 0x4C:
            if (s->bConfig) {
                if (cb[3] == 0x00) memcpy(&db[4], DUALSHOCK2_ID[3], 5);
                else                memcpy(&db[4], DUALSHOCK2_ID[4], 5);
            }
            break;

        default:
            break;
    }
}

/* Cmd8: ultimo byte recibido.  Para 0x42 dispara la vibracion.  Para
 * 0x4D guarda el rumble mapping completo.  Equivalente a Pokopom
 * DualShock::Cmd8. */
static void ds_cmd8(ds_state_t *s, uint8_t cmd, int port)
{
    uint8_t *cb = s->cmdBuffer;

    switch (cmd) {
        case 0x42: {
            /* Aplicar rumble usando el mapping previamente configurado
             * por 0x4D.  Si motorMap* == 0xFF, no hay motor mapeado en
             * ese slot — pasar 0 al callback. */
            uint8_t small = (s->motorMapS == 0xFF) ? 0 : cb[s->motorMapS + 3];
            uint8_t big   = (s->motorMapL == 0xFF) ? 0 : cb[s->motorMapL + 3];
            if (s_rumble_cb)
                s_rumble_cb(port, small, big);
            break;
        }

        case 0x4D: {
            /* Set vibration mapping: cmdBuffer[3..8] tiene 6 bytes con
             * 0x00 (small en este slot), 0x01 (big aqui) o 0xFF (nada).
             * Encontramos en qué slots estan small y big. */
            uint8_t i;
            s->motorMapS = 0xFF;
            s->motorMapL = 0xFF;
            for (i = 3; i < 9; i++) {
                if (cb[i] == 0x00) s->motorMapS = i - 3;
                if (cb[i] == 0x01) s->motorMapL = i - 3;
            }
            break;
        }

        default:
            break;
    }
}

/* =========================================================================
 *  Punto de entrada principal
 * ========================================================================= */

uint8_t ds_command(int port, uint32_t counter, uint8_t data)
{
    ds_state_t *s;

    if (port < 0 || port >= DS_NUM_PORTS) return 0xFF;
    if (counter >= DS_BUF_SIZE)            return 0x00;
    s = &s_state[port];

    s->cmdBuffer[counter] = data;

    switch (counter) {
        case 0x00:
            /* Start byte (0x01 del juego).  Pokopom hace toggle analog
             * con boton fisico aqui; nosotros omitimos.  Solo retornamos
             * el header (dataBuffer[0] = 0xFF). */
            break;

        case 0x01:
            /* Comando recibido.  Decidir ID del pad para responder. */
            s->dataBuffer[1] = s->bConfig ? ID_CONFIG : s->padID;
            ds_cmd1(s, s->cmdBuffer[1]);
            break;

        case 0x02:
            /* Post-id constant. */
            s->dataBuffer[2] = 0x5A;
            break;

        case 0x04:
            /* Bytes 3 y 4 recibidos — procesar comandos que dependan de
             * ellos (config toggle, set mode, etc.). */
            ds_cmd4(s, s->cmdBuffer[1]);
            break;

        case 0x08:
            /* Ultimo byte (8) recibido — disparar vibracion / capturar
             * mapping.  Pokopom hace solo aqui, no per-byte. */
            ds_cmd8(s, s->cmdBuffer[1], port);
            break;

        default:
            /* Counters 3, 5, 6, 7: solo capturar el byte (ya lo hicimos
             * arriba) y devolver lo que dataBuffer dice. */
            break;
    }

    return s->dataBuffer[counter];
}
