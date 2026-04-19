#include <string>
#include "libretro/libretro.h"
#include "utils/langmanager.h"

//disk_control.set_eject_state(true) para abrir la bandeja.
//disk_control.replace_image_index(index, &info) para cambiar el CHD.
struct retro_disk_control_callback disk_control;
struct retro_disk_control_ext_callback disk_control_ext;

// ─────────────────────────────────────────────
// Multi-disc helpers: persist last selected disc
// next to the .m3u as "<rompath>.disc"
// ─────────────────────────────────────────────
static unsigned loadLastDiscIndex(const std::string& rompath) {
    std::string p = rompath + ".disc";
    FILE* f = fopen(p.c_str(), "r");
    if (!f) return 0;
    unsigned idx = 0;
    fscanf(f, "%u", &idx);
    fclose(f);
    return idx;
}

static void saveLastDiscIndex(const std::string& rompath, unsigned idx) {
    if (rompath.empty()) return;
    std::string p = rompath + ".disc";
    FILE* f = fopen(p.c_str(), "w");
    if (!f) return;
    fprintf(f, "%u\n", idx);
    fclose(f);
}

// Attempt disc swap via the core-provided disk control interface.
// Returns true if the swap was actually performed.
bool swapDisc(unsigned new_idx) {
    if (!disk_control.get_num_images || !disk_control.set_eject_state ||
        !disk_control.set_image_index) {
        return false;
    }
    unsigned n = disk_control.get_num_images();
    if (n <= 1 || new_idx >= n) return false;
    unsigned cur = disk_control.get_image_index ? disk_control.get_image_index() : 0;
    if (new_idx == cur) return false;

    disk_control.set_eject_state(true);
    disk_control.set_image_index(new_idx);
    disk_control.set_eject_state(false);

    // OSD feedback
    char buf[64];
    _snprintf(buf, sizeof(buf), LanguageManager::instance()->get("msg.cd.discnum").c_str(), new_idx + 1, n);
    buf[sizeof(buf) - 1] = '\0';
    if (gameMenu) gameMenu->showSystemMessage(buf, 1500);

	gameMenu->setEmuStatus(EMU_STARTED);
	gameMenu->clearOverlay();
    return true;
}

// Swap al vuelo a un .bin/.chd arbitrario escogido por el usuario
// (juegos sin M3U que piden cambio de CD).
//
// Estrategia: el slot 0 es el disco original con el que se cargo el juego;
// reutilizamos el slot 1 para todos los cambios futuros (crece la lista
// solo una vez, evita saturar DISK_MAX_IMAGES).
bool swapToNewDisc(const std::string& newBinPath) {
    if (newBinPath.empty()) return false;
    if (!disk_control.get_num_images    || !disk_control.add_image_index   ||
        !disk_control.replace_image_index || !disk_control.set_image_index ||
        !disk_control.set_eject_state) {
        if (gameMenu) gameMenu->showLangSystemMessage("msg.cd.dynswap", 2500);
        return false;
    }

    // 1. Escoger slot destino: si solo hay 1 disco (el original), ampliamos
    //    la lista a 2 y usamos el nuevo slot 1. Si ya hay >= 2 discos,
    //    reutilizamos el slot 1 pisando su ruta anterior.
    unsigned target_idx = disk_control.get_num_images();   // sera 1 la primera vez
    if (target_idx >= 1) target_idx = 1;                   // reuso de slot 1

    if (target_idx >= disk_control.get_num_images()) {
        if (!disk_control.add_image_index()) {
            if (gameMenu) gameMenu->showLangSystemMessage("msg.cd.add_image_index", 2000);
            return false;
        }
    }

    // 2. Asignar la ruta al slot
    struct retro_game_info gi;
    memset(&gi, 0, sizeof(gi));
    gi.path = newBinPath.c_str();   // el core abre el fichero por su cuenta
    gi.data = NULL;
    gi.size = 0;
    gi.meta = NULL;

    if (!disk_control.replace_image_index(target_idx, &gi)) {
        if (gameMenu) gameMenu->showLangSystemMessage("msg.cd.replace_image_index", 2000);
        return false;
    }

    // 3. Triada de swap: eject -> set_image_index -> uneject.
    //    Los juegos PSX solo detectan el cambio en la transicion
    //    cerrado->abierto->cerrado de la bandeja.
    disk_control.set_eject_state(true);
    disk_control.set_image_index(target_idx);
    disk_control.set_eject_state(false);

    // OSD feedback: mostrar solo el nombre del fichero, no la ruta completa
    std::size_t sep = newBinPath.find_last_of("/\\");
    std::string fname = (sep == std::string::npos) ? newBinPath : newBinPath.substr(sep + 1);
    char buf[160];
	_snprintf(buf, sizeof(buf), LanguageManager::instance()->get("msg.cd.dischanged").c_str(), fname.c_str());
    buf[sizeof(buf) - 1] = '\0';
    if (gameMenu) gameMenu->showSystemMessage(buf, 2000);

	gameMenu->setEmuStatus(EMU_STARTED);
	gameMenu->clearOverlay();
    return true;
}

void detectContainer(std::string rompath, bool& isM3U, bool& isContainer){
	// Detectar contenedores que REFERENCIAN otros ficheros por ruta relativa:
	//   .m3u  -> playlist de discos
	//   .cue  -> descriptor de pistas que apunta a un .bin/.wav externo
	//   .ccd  -> CloneCD, apunta a .img
	//   .toc  -> cdrdao, apunta a .bin
	//   .pbp  -> EBOOT PSP/PSX (multi-disco empaquetado)
	// Estos NO se pueden cargar en un buffer de memoria porque el core pierde
	// la ruta base para resolver los ficheros companeros. Hay que pasarle
	// la ruta real al disco y que el core la abra via VFS.
	isM3U        = false;
	isContainer  = false;
	std::size_t dot = rompath.find_last_of('.');
	if (dot != std::string::npos) {
		std::string e = rompath.substr(dot + 1);
		for (std::size_t i = 0; i < e.size(); ++i)
			e[i] = (char)tolower((unsigned char)e[i]);
		isM3U = (e == "m3u");
		isContainer = isM3U || (e == "cue") || (e == "ccd") ||
			            (e == "toc") || (e == "pbp");
	}
}

void findInitialImage(std::string rompath, bool isM3U){
	// Multi-disc: si es un M3U, indicamos al core qué disco cargar de inicio
	// antes de retro_load_game (asi el savestate coincide con el disco correcto).
	if (isM3U && disk_control_ext.set_initial_image) {
		unsigned saved_idx = loadLastDiscIndex(rompath);
		disk_control_ext.set_initial_image(saved_idx, rompath.c_str());
		LOG_DEBUG("Disc control: set_initial_image(%u, %s)", saved_idx, rompath.c_str());
	}
}
	