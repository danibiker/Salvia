#pragma once

/* ==========================================================================
 * PANEL A 50 Hz PARA LOS JUEGOS PAL (solo Xbox 360)
 *
 * Un juego PAL (49-51 fps) en un panel a 60 Hz sale con la cadencia 5:6: un
 * frame de cada cinco se ve dos veces y el scroll da saltos.  Con el panel a
 * 50 Hz cada frame va a un vblank y Sync lo ata al panel (r = 1) corrigiendo
 * el audio con el skew (49,70 fps en Mega Drive -> skew 1,006).
 *
 * El cambio lo hace SDL_XBOX_SetRefreshOverride (SDL_xboxvideo.c): forzado
 * del kernel con el mismo ancho, alto y barrido de la salida, y un Reset del
 * device, que es lo que de verdad cambia la salida.  Aqui solo se decide
 * CUANDO, segun la opcion cfg::refresh50Hz:
 *
 *   REFRESH50_NO       nunca.
 *   REFRESH50_AUTO     si en el panel de control de la consola se dijo que la
 *                      pantalla admite 50 Hz (SDL_XBOX_DisplaySupports50Hz).
 *   REFRESH50_CONFIRM  siempre, sin mirar el panel de control, pero se
 *                      mantiene solo si se pulsa A en 10 s: si la tele se
 *                      queda sin senal, vuelve sola a 60 Hz.
 *
 * Momentos:
 *   - Al cargar el juego (refresco50AlCargar, desde launchGame, con la pantalla
 *     de carga puesta y antes del primer frame).  Entre dos juegos PAL seguidos
 *     el panel se queda a 50 Hz: closeGame no lo quita, lo decide la carga
 *     siguiente, y asi la tele no cambia de modo dos veces.
 *   - Si el core cambia de fps a mitad de partida (refresco50AvInfo, desde
 *     SET_SYSTEM_AV_INFO): puede llegar dentro de retro_run, asi que solo se
 *     anota y lo hace refresco50Tick entre frames.
 *   - Al salir de Salvia (refresco50AlSalir) y antes de lanzar otro XEX
 *     (Launcher): se quita sin Reset, el siguiente ejecutable aplica el modo
 *     al crear su device.
 *
 * Cores que dibujan por hardware (los que piden el device, env 30002): en la
 * 360 el Reset no destruye texturas ni superficies, pero SUSTITUYE el back
 * buffer.  Un core que lo guarde se queda con el viejo y la imagen sale
 * partida en bloques o negra; tiene que pedirlo al device cada vez que lo use.
 * pcsxr-360 (SwanStation HW) siempre lo ha hecho, y Xenon64 lo hace desde
 * xenon64_video_frame_end.  Todos los Reset de aqui ocurren entre frames, con
 * retro_run ya devuelto.
 * ========================================================================== */

#ifdef _XBOX

#include <SDL.h>
#include <utils/logger.h>
#include <const/constant.h>
#include <const/cfgconst.h>
#include <io/cfgloader.h>
#include <menus/gameMenu.h>
#include <utils/langmanager.h>

extern GameMenu *gameMenu;

enum { R50_NADA, R50_PENDIENTE, R50_CONFIRMANDO, R50_ACTIVO };
static int   r50Estado          = R50_NADA;
static bool  r50QuitarPendiente = false;
static DWORD r50Inicio          = 0;
static WORD  r50Botones         = 0;

static int  r50Modo()            { return CfgLoader::configMain[cfg::refresh50Hz].valueInt; }
static bool r50EsPal(double fps) { return fps >= 49.0 && fps <= 51.0; }
static bool r50A50()             { return r50Estado == R50_CONFIRMANDO || r50Estado == R50_ACTIVO; }

static bool r50Permitido() {
	switch (r50Modo()) {
		case REFRESH50_AUTO:    return SDL_XBOX_DisplaySupports50Hz() != 0;
		case REFRESH50_CONFIRM: return true;
		default:                return false;
	}
}

static std::string r50Texto(const char* clave, const char* porDefecto) {
	std::string s = LanguageManager::instance()->get(clave);
	return (s.empty() || s[0] == '[') ? std::string(porDefecto) : s;
}

static WORD r50LeeBotones() {
	WORD b = 0;
	for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
		XINPUT_STATE st;
		if (XInputGetState(i, &st) == ERROR_SUCCESS) b |= st.Gamepad.wButtons;
	}
	return b;
}

static void r50Aplicar() {
	if (!SDL_XBOX_SetRefreshOverride(50.0f, 1)) {
		LOG_INFO("Refresco 50 Hz: el kernel no tiene un modo de 50 Hz para esta salida");
		r50Estado = R50_NADA;
		return;
	}
	gameMenu->sync->invalidateDeviceState();   //relee el refresco y reaplica el vsync

	if (r50Modo() == REFRESH50_CONFIRM) {
		gameMenu->showSystemMessage(r50Texto("msg.refresh50.confirm", "50 Hz: press A within 10 seconds to keep it"), 10000);
		r50Botones = r50LeeBotones();
		r50Inicio  = GetTickCount();
		r50Estado  = R50_CONFIRMANDO;
		LOG_INFO("Refresco 50 Hz: panel a %.2f Hz, esperando confirmacion", SDL_XBOX_GetRefreshRate());
	} else {
		r50Estado  = R50_ACTIVO;
		LOG_INFO("Refresco 50 Hz: panel a %.2f Hz", SDL_XBOX_GetRefreshRate());
	}
}

static void r50Quitar(const char* motivo, bool reset) {
	r50QuitarPendiente = false;
	if (r50Estado == R50_PENDIENTE) { r50Estado = R50_NADA; return; }
	if (!r50A50()) return;
	r50Estado = R50_NADA;
	SDL_XBOX_SetRefreshOverride(0.0f, reset ? 1 : 0);
	if (reset) gameMenu->sync->invalidateDeviceState();
	LOG_INFO("Refresco 50 Hz: panel devuelto a %.2f Hz (%s)", SDL_XBOX_GetRefreshRate(), motivo);
}

/* launchGame, con el juego ya cargado: decide el panel para este juego */
static void refresco50AlCargar(double fps) {
	if (r50EsPal(fps) && r50Permitido()) {
		if (!r50A50()) r50Aplicar();   //si ya esta a 50 Hz (juego PAL anterior), se queda
	} else {
		r50Quitar("juego a 60 Hz", true);
	}
}

/* launchGame, si la carga falla: closeGame ya cerro el anterior y no hay juego */
static void refresco50AlFallarCarga() {
	r50Quitar("sin juego", true);
}

/* applyCoreAvInfo: el core cambia de fps a mitad de partida */
static void refresco50AvInfo(double fps) {
	if (!gameMenu->romLoaded) return;   //la carga lo decide refresco50AlCargar
	if (r50EsPal(fps)) {
		r50QuitarPendiente = false;
		if (r50Estado == R50_NADA && r50Permitido()) r50Estado = R50_PENDIENTE;
	} else if (r50A50()) {
		r50QuitarPendiente = true;
	} else if (r50Estado == R50_PENDIENTE) {
		r50Estado = R50_NADA;
	}
}

/* Bucle principal, entre frames, en el juego y en los menus */
static void refresco50Tick() {
	if (r50QuitarPendiente) {
		r50Quitar("el core ya no va a 50 fps", true);
	} else if (r50A50() && r50Modo() == REFRESH50_NO) {
		r50Quitar("opcion desactivada", true);
	} else if (r50Estado == R50_PENDIENTE) {
		if (gameMenu->getEmuStatus() == EMU_STARTED) r50Aplicar();
	} else if (r50Estado == R50_CONFIRMANDO) {
		const WORD b = r50LeeBotones();
		const WORD nuevos = b & ~r50Botones;
		r50Botones = b;
		if (nuevos & XINPUT_GAMEPAD_A) {
			r50Estado = R50_ACTIVO;
			gameMenu->showSystemMessage(r50Texto("msg.refresh50.confirmed", "50 Hz confirmed"), 2000);
			LOG_INFO("Refresco 50 Hz: confirmado");
		} else if (GetTickCount() - r50Inicio > 10000) {
			r50Quitar("sin confirmar en 10 s", true);
		}
	}
}

/* Al salir de Salvia: sin Reset, el siguiente ejecutable aplica el modo */
static void refresco50AlSalir() {
	r50Quitar("salida", false);
}

#else  /* Windows: el escritorio decide el refresco */

static inline void refresco50AlCargar(double)  {}
static inline void refresco50AlFallarCarga()   {}
static inline void refresco50AvInfo(double)    {}
static inline void refresco50Tick()            {}
static inline void refresco50AlSalir()         {}

#endif
