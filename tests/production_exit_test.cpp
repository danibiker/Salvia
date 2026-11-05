// API stand-ins around source bodies extracted by run_host_tests.py. No XDK ABI
// or real SDL/core behavior is inferred from this fixture.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <unistd.h>
#include "libretro/libretro.h"
#include "so/returnapp.h"
#include "io/rawsave.h"

#ifdef NDEBUG
#error Host tests require assertions
#endif
#define __declspec(value)
#define LOG_ERROR(...) do {} while (0)
#define LOG_DEBUG(...) do {} while (0)
typedef uint32_t Uint32;
typedef uint32_t DWORD;
const DWORD INVALID_FILE_ATTRIBUTES = 0xffffffffu;
const DWORD FILE_ATTRIBUTE_DIRECTORY = 0x10;
const DWORD ERROR_SUCCESS = 0;
const int MAX_PLAYERS = 4, SDL_HAT_UP = 1, SDL_HAT_DOWN = 4, MAX_SAVESTATES = 10;
const int ico_resume = 1, clBackground = 0;

#include "production_types.inc"

static std::vector<std::string> events;
static std::string scratch, savePath;
static Uint32 ticks, drainStarted;
static int drainAfter, delays, frameLimit, inputMode, frames, gameUpdates;
static bool insideCallback, nullRam, unloaded, replaceProcess, pauseAllowed, mutexLockFails, mutexUnlockFails;
static DWORD targetAttributes, clearDataResult;
static std::vector<unsigned char> ram;
enum Failure { NO_FAILURE, FAIL_OPEN, FAIL_WRITE, FAIL_FLUSH, FAIL_COMMIT, FAIL_CLOSE };
static Failure failure;
static int commitCalls;

static void note(const char* text) { events.push_back(text); }
static int count(const std::string& text) {
    int result = 0;
    for (std::size_t i = 0; i < events.size(); ++i) if (events[i] == text) ++result;
    return result;
}
static std::size_t at(const std::string& text) {
    for (std::size_t i = 0; i < events.size(); ++i) if (events[i] == text) return i;
    assert(!"Missing expected event");
    return 0;
}
static std::vector<unsigned char> readSave() {
    std::ifstream input(savePath.c_str(), std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

extern "C" FILE* __real_fopen(const char*, const char*);
extern "C" size_t __real_fwrite(const void*, size_t, size_t, FILE*);
extern "C" int __real_fflush(FILE*);
extern "C" int __real_fclose(FILE*);
extern "C" FILE* __wrap_fopen(const char* path, const char* mode) {
    note("open");
    return failure == FAIL_OPEN ? NULL : __real_fopen(path, mode);
}
extern "C" size_t __wrap_fwrite(const void* data, size_t size, size_t amount, FILE* file) {
    note("write");
    return __real_fwrite(data, size, failure == FAIL_WRITE && amount ? amount - 1 : amount, file);
}
extern "C" int __wrap_fflush(FILE* file) {
    note("flush");
    const int result = __real_fflush(file);
    return failure == FAIL_FLUSH ? EOF : result;
}
extern "C" int __wrap_fclose(FILE* file) {
    note("file-close");
    const int result = __real_fclose(file); // Always release the real file, even when simulating failure.
    return failure == FAIL_CLOSE ? EOF : result;
}
int _commit(int descriptor) {
    ++commitCalls;
    note("commit");
    return failure == FAIL_COMMIT ? -1 : fsync(descriptor);
}

struct SDL_mutex { bool locked; int locks, unlocks; SDL_mutex() : locked(false), locks(0), unlocks(0) {} };
struct SaveQueue { SDL_mutex* saveMutex; void* buffer; } g_saveQueue;
static int queuedToken;
int SDL_mutexP(SDL_mutex* mutex) {
    assert(mutex && !mutex->locked);
    if (mutexLockFails) { note("mutex-lock-error"); return -1; }
    mutex->locked = true; ++mutex->locks; return 0;
}
int SDL_mutexV(SDL_mutex* mutex) {
    assert(mutex && mutex->locked);
    mutex->locked = false; ++mutex->unlocks;
    if (mutexUnlockFails) { note("mutex-unlock-error"); return -1; }
    return 0;
}
Uint32 SDL_GetTicks() { return ticks; }
void SDL_Delay(Uint32 amount) {
    assert(!g_saveQueue.saveMutex || !g_saveQueue.saveMutex->locked);
    ticks += amount;
    if (amount != 1) return;
    ++delays;
    if (g_saveQueue.buffer && drainAfter >= 0 && ticks - drainStarted >= static_cast<Uint32>(drainAfter)) {
        // Deterministic worker completion, not an SDL thread/scheduling model.
        const unsigned char oldSnapshot[] = {'O', 'L', 'D'};
        assert(writeRawSave(savePath.c_str(), oldSnapshot, sizeof(oldSnapshot)));
        SDL_mutexP(g_saveQueue.saveMutex);
        g_saveQueue.buffer = NULL;
        SDL_mutexV(g_saveQueue.saveMutex);
        note("worker-complete");
    }
}
void SDL_PauseAudio(int value) { note(value ? "audio-pause" : "audio-resume"); }
struct SDL_Surface {} surface;
void SDL_FreeSurface(SDL_Surface*) { note("screenshot-free"); }

namespace Constant {
std::string checkPath(const char*) { note("check-save-path"); return savePath; }
double getTicks() { return ticks; }
}
size_t retro_get_memory_size(unsigned type) {
    assert(type == RETRO_MEMORY_SAVE_RAM && !unloaded && !g_saveQueue.buffer);
    note("ram-size"); return ram.size();
}
void* retro_get_memory_data(unsigned type) {
    assert(type == RETRO_MEMORY_SAVE_RAM && !unloaded && !g_saveQueue.buffer);
    note("ram-data"); return nullRam || ram.empty() ? NULL : &ram[0];
}
void retro_unload_game() { assert(!insideCallback && !unloaded); note("unload"); unloaded = true; ram.clear(); }
void retro_deinit() { assert(!insideCallback && unloaded); note("deinit"); }

namespace cfg {
struct t_cfg_props {
    std::string valueStr, desc;
    t_cfg_props() {}
    t_cfg_props(const char*, const std::string& value) : valueStr(value) {}
};
}
struct CfgLoader {
    static cfg::t_cfg_props configMain[cfg::MAIN_CFG_MAX];
    void initReturnConfigForTest();
    int emuCfgPos;
    struct { int sync_type; } current_video_settings;
};
cfg::t_cfg_props CfgLoader::configMain[cfg::MAIN_CFG_MAX];
struct Mapper {
    int buttons[MAX_PLAYERS][HK_MAX], hats[MAX_PLAYERS][HK_MAX];
    Mapper() { for (int p = 0; p < MAX_PLAYERS; ++p) for (int h = 0; h < HK_MAX; ++h) buttons[p][h] = hats[p][h] = -1; }
    void setBtnFromSdl(int p, int b, int h) { buttons[p][h] = b; }
    void setHatFromSdl(int p, int b, int h) { hats[p][h] = b; }
    int getSdlBtn(int p, int h) { return buttons[p][h]; }
    int getSdlHat(int p, int h) { return hats[p][h]; }
};
struct t_joy_state {
    Mapper mapperHotkeys;
    bool btn_state[MAX_PLAYERS][64], hats_state[MAX_PLAYERS][16];
    t_joy_state() { clearAll(); }
    void clearAll() { std::memset(btn_state, 0, sizeof(btn_state)); std::memset(hats_state, 0, sizeof(hats_state)); }
    bool getSdlBtn(int p, int b) { return btn_state[p][b]; }
    bool getSdlHat(int p, int h) { return hats_state[p][h]; }
};
struct Joystick { t_joy_state inputs; };
class Hotkeys {
public:
    Hotkeys(t_joy_state* inputs);
    HOTKEYS_LIST procesarHotkeys(t_joy_state* inputs);
};
struct OpcionBase { virtual ~OpcionBase() {} virtual std::string execute() = 0; };
class GestorMenus;
template<class T> struct OpcionExec : OpcionBase {
    std::string (GestorMenus::*callback)(T*);
    T* data;
    GestorMenus* manager;
    OpcionExec(const std::string&, std::string (GestorMenus::*fn)(T*), T* value, int, GestorMenus* owner)
        : callback(fn), data(value), manager(owner) {}
    std::string execute() { return (manager->*callback)(data); }
};
struct Menu {
    std::vector<OpcionBase*> opciones;
    ~Menu() { for (std::size_t i = 0; i < opciones.size(); ++i) delete opciones[i]; }
};
class GestorMenus {
public:
    CONFIG_STATUS status;
    Menu root;
    Menu* menuRaiz;
    std::string gameName;
    GestorMenus() : status(NORMAL), menuRaiz(&root) {}
    void buildReturnEntryForTest(CfgLoader*);
    std::string volverAplicacion(CONFIG_STATUS*);
    std::string volverEmulacion(CONFIG_STATUS*);
    std::string salirEmulacion(CONFIG_STATUS*);
    std::string trOrDefault(const std::string&, const char* fallback) { return fallback; }
    CONFIG_STATUS getStatus() { return status; }
    void resetStatus() { status = NORMAL; }
    void volverMenuInicial() {}
    void poblarPartidasGuardadas(CfgLoader*, const std::string&) {}
    void setGameLoaded(const std::string& name) { gameName = name; }
};
struct Rate { void reset() { note("rate-reset"); } };
struct AudioBuffer { void Clear() { note("audio-clear"); } };
struct Midi { void panic() { note("midi-panic"); } void close() { note("midi-close"); } };
struct Sync {
    void note_flip(double) {}
    void note_present(double) {}
    void limit_fps(double&, int, int) {}
};
struct GameMenu {
    bool running, romLoaded, returnAppRequested;
    int status, lastStatus, gameTicks;
    SDL_Surface *bg_screenshot, *gameScreen, *overlay;
    Joystick* joystick;
    GestorMenus* configMenus;
    CfgLoader* cfgLoader;
    Sync* sync;
    AudioBuffer g_audioBuffer;
    Rate g_audioRate;
    Midi g_midi;
    std::vector<std::string> messages;
    void exitHotkeyForTest(int key);
    void setEmuStatus(int value) { lastStatus = status; status = value; joystick->inputs.clearAll(); note("status"); }
    int getEmuStatus() { return status; }
    int getLastStatus() { return lastStatus; }
    CfgLoader* getCfgLoader() { return cfgLoader; }
    void clearOverlay() { note("overlay-clear"); }
    void fillOverlay(int) {}
    SDL_Surface* clonarPantalla(SDL_Surface*, int) { note("screenshot-clone"); return &surface; }
    void showSystemMessage(const std::string& text, int) { messages.push_back(text); }
    void showLangSystemMessage(const std::string& text, int) { messages.push_back(text); }
    void startScrapping() {}
    void drawLightgunCrosshair() {}
    void processFrontendEventsAfter() { if (++frames >= frameLimit) running = false; }
};
GameMenu* gameMenu;
CfgLoader* cfgLoader;
struct Music { bool active; bool isActive() { return active; } void setWanted(bool) {} } *g_music;
struct LanguageManager {
    static LanguageManager* instance() { static LanguageManager value; return &value; }
    std::string get(const char* key) { return key; }
};
struct Achievements {
    static Achievements* instance() { static Achievements value; return &value; }
    bool canPause() { return pauseAllowed; }
};
struct CheatManager {
    static CheatManager* instance() { static CheatManager value; return &value; }
    void clear() { note("cheats-clear"); }
};
struct { std::string sram, rompath; } romPaths;
struct { int cycles; ThreadAction action; } action_postponed;
int g_currentSlot;
void loadState() {}
struct dirutil { void borrarArchivo(const std::string&) {} };
int audio_opened, g_singleSampleFrames;
bool audio_closing, g_midiFastForwardCut;
std::string g_currentRompath;
retro_disk_control_callback disk_control;
unsigned g_num_mem_descriptors;
retro_memory_descriptor g_mem_descriptors[2];
void saveSram(const char*) { note("async-save"); }
void saveLastDiscIndex(const std::string&, unsigned) { note("disc-save"); }
void closeGame(bool sramAlreadySaved = false);
class Launcher { public: void launchReturnApp(const std::string&); };
struct ProcessReplaced {};
DWORD GetFileAttributes(const char* path) {
    assert(std::string(path) == "Hdd:\\Apps\\Front End\\default.xex");
    note("target-check"); return targetAttributes;
}
DWORD XSetLaunchData(void* data, DWORD bytes) {
    assert(!insideCallback && data == NULL && bytes == 0);
    note("clear-launch-data"); return clearDataResult;
}
void XLaunchNewImage(const char* path, DWORD flags) {
    assert(!insideCallback && unloaded && !gameMenu->romLoaded && flags == 0);
    assert(std::string(path) == "Hdd:\\Apps\\Front End\\default.xex");
    note("launch");
    if (replaceProcess) throw ProcessReplaced();
}
int tileMap, listStorage, *listMenu = &listStorage;
double nextFrameTime;
bool musicWantedFor(int) { return true; }
void applyMenuMusic() {}
void updateGame() { assert(!unloaded && gameMenu->romLoaded); ++gameUpdates; }
void updateMenuScreen(int, GameMenu*, int) { note("menu-frame"); }
void updateMenuOverlay(GameMenu*, int) {}
void salviaFlip(SDL_Surface*) {}
void processFrontendEvents();

#include "production_functions.inc"

static Hotkeys* activeHotkeys;
enum { INPUT_NONE, INPUT_HOTKEY, INPUT_MENU, INPUT_QUIT };
void processFrontendEvents() {
    insideCallback = true;
    if (frames == 0 && inputMode == INPUT_HOTKEY)
        gameMenu->exitHotkeyForTest(activeHotkeys->procesarHotkeys(&gameMenu->joystick->inputs));
    else if (frames == 0 && inputMode == INPUT_MENU) {
        assert(gameMenu->configMenus->root.opciones.size() == 1);
        gameMenu->configMenus->root.opciones[0]->execute();
        t_option_action action;
        action.elem = std::malloc(4);
        assert(action.elem);
        processActions(gameMenu, action);
        assert(action.elem == NULL);
    } else if (frames == 0 && inputMode == INPUT_QUIT) {
        gameMenu->returnAppRequested = true;
        gameMenu->running = false;
    }
    assert(count("launch") == 0 || frames > 0);
    note("callback-end");
    insideCallback = false;
}

struct Fixture {
    SDL_mutex mutex;
    CfgLoader cfg;
    GestorMenus menus;
    Joystick joystick;
    Sync sync;
    GameMenu game;
    Music music;
    Hotkeys keys;
    Fixture() : keys(&joystick.inputs) {
        static Uint32 epoch = 10000;
        ticks = epoch; epoch += 10000;
        drainStarted = ticks; drainAfter = -1; delays = frames = gameUpdates = 0;
        frameLimit = 2; inputMode = INPUT_NONE; insideCallback = false; nullRam = unloaded = replaceProcess = false;
        pauseAllowed = true; mutexLockFails = mutexUnlockFails = false;
        failure = NO_FAILURE; targetAttributes = clearDataResult = 0; commitCalls = 0;
        ram.assign({'N', 'E', 'W'});
        gameMenu = &game; cfgLoader = &cfg; activeHotkeys = &keys; g_music = NULL;
        cfg.emuCfgPos = 0; cfg.current_video_settings.sync_type = 0;
        for (int i = 0; i < cfg::MAIN_CFG_MAX; ++i) cfg.configMain[i] = cfg::t_cfg_props();
        game.cfgLoader = &cfg; game.configMenus = &menus; game.joystick = &joystick; game.sync = &sync;
        game.running = game.romLoaded = true; game.returnAppRequested = false;
        game.status = game.lastStatus = EMU_STARTED; game.gameTicks = 0;
        game.bg_screenshot = NULL; game.gameScreen = game.overlay = &surface;
        g_saveQueue.saveMutex = &mutex; g_saveQueue.buffer = NULL;
        action_postponed.cycles = 1; action_postponed.action = SAVE_STATE;
        audio_opened = 0; audio_closing = false; g_singleSampleFrames = 7; g_midiFastForwardCut = true;
        g_currentRompath = "fixture.rom"; romPaths.rompath = "fixture.rom"; romPaths.sram = "game:\\fixture.srm";
        std::memset(&disk_control, 0, sizeof(disk_control));
        g_num_mem_descriptors = 2; std::memset(g_mem_descriptors, 0xa5, sizeof(g_mem_descriptors));
        menus.gameName = "fixture";
        savePath = scratch + "/production-exit-test.srm";
        std::remove(savePath.c_str());
        events.clear();
    }
    ~Fixture() {
        assert(!mutex.locked && mutex.locks == mutex.unlocks);
        std::remove(savePath.c_str());
        gameMenu = NULL; cfgLoader = NULL; g_saveQueue.saveMutex = NULL; g_saveQueue.buffer = NULL;
    }
    void configure() { cfg.configMain[cfg::returnApp].valueStr = "Hdd:/Apps/Front End/default.xex"; }
    void queue(int completeAfter) { g_saveQueue.buffer = &queuedToken; drainAfter = completeAfter; drainStarted = ticks; }
    void pressExit() {
        joystick.inputs.btn_state[0][joystick.inputs.mapperHotkeys.getSdlBtn(0, HK_MODIFIER)] = true;
        joystick.inputs.btn_state[0][joystick.inputs.mapperHotkeys.getSdlBtn(0, HK_EXIT_GAME)] = true;
    }
};
static int passed;
static void test(const char* name, const std::function<void()>& body) {
    body(); ++passed; std::cout << "ok: " << name << '\n';
}
static void noHandoff() { assert(count("unload") == 0 && count("deinit") == 0 && count("launch") == 0); }

static void drainTests() {
    test("missing save mutex rejects exit", [] { Fixture f; g_saveQueue.saveMutex = NULL; assert(!saveSramForExit("x", true)); noHandoff(); assert(events.empty()); });
    test("mutex lock failure rejects exit without unlocking unowned mutex", [] {
        Fixture f; mutexLockFails = true; assert(!saveSramForExit("x", true));
        assert(f.mutex.locks == 0 && f.mutex.unlocks == 0 && count("ram-size") == 0); noHandoff();
    });
    test("reported mutex unlock error rejects exit before RAM access", [] {
        Fixture f; mutexUnlockFails = true; assert(!saveSramForExit("x", true));
        assert(count("ram-size") == 0 && count("write") == 0); noHandoff();
    });
    test("idle queue saves final RAM", [] {
        Fixture f; assert(saveSramForExit("x", true)); assert(delays == 0);
#ifndef NO_SRAM
        assert(readSave() == ram); assert(at("ram-size") < at("write"));
#endif
    });
    test("busy old snapshot finishes before final RAM write", [] {
        Fixture f; f.queue(3); assert(saveSramForExit("x", true)); assert(delays == 3);
#ifndef NO_SRAM
        assert(readSave() == ram); assert(at("worker-complete") < at("ram-size"));
#else
        assert(readSave() == std::vector<unsigned char>({'O', 'L', 'D'}));
#endif
    });
    test("stalled save times out at five seconds", [] {
        Fixture f; f.queue(-1); const Uint32 start = ticks;
        assert(!saveSramForExit("x", true)); assert(ticks - start == 5000 && delays == 5000);
        assert(count("ram-size") == 0 && count("write") == 0); noHandoff();
    });
    test("timeout handles unsigned tick wrap", [] {
        Fixture f; ticks = 0xfffffff0u; f.queue(-1); const Uint32 start = ticks;
        assert(!saveSramForExit("x", true)); assert(ticks - start == 5000); noHandoff();
    });
    test("worker completion just before deadline succeeds", [] { Fixture f; f.queue(4999); assert(saveSramForExit("x", true)); assert(delays == 4999); });
    test("worker completion past deadline cannot unload", [] { Fixture f; f.queue(5001); assert(!saveSramForExit("x", true)); assert(delays == 5000); noHandoff(); });
    test("no loaded ROM still drains pending work", [] {
        Fixture f; f.queue(2); assert(saveSramForExit(NULL, false)); assert(delays == 2 && count("ram-size") == 0);
    });
    test("no loaded ROM and idle queue does not query core", [] { Fixture f; assert(saveSramForExit(NULL, false)); assert(events.empty()); });
#ifndef NO_SRAM
    test("zero SRAM size skips disk writes", [] { Fixture f; ram.clear(); assert(saveSramForExit(NULL, true)); assert(count("open") == 0); });
    test("null SRAM buffer cancels exit", [] { Fixture f; nullRam = true; assert(!saveSramForExit("x", true)); assert(count("open") == 0); });
    const Failure faults[] = {FAIL_OPEN, FAIL_WRITE, FAIL_FLUSH, FAIL_CLOSE};
    for (std::size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
        const Failure fault = faults[i];
        test("actual final SRAM writer reports injected I/O failure", [fault] {
            Fixture f; failure = fault; assert(!saveSramForExit("x", true)); noHandoff();
            assert(count("file-close") == (fault == FAIL_OPEN ? 0 : 1));
        });
    }
#ifdef _XBOX
    test("commit failure cancels exit and closes file", [] { Fixture f; failure = FAIL_COMMIT; assert(!saveSramForExit("x", true)); assert(commitCalls == 1 && count("file-close") == 1); });
#endif
#else
    test("NO_SRAM does not query core or write final RAM", [] { Fixture f; failure = FAIL_OPEN; assert(saveSramForExit(NULL, true)); assert(events.empty()); });
#endif
}

static void inputTests() {
    test("return target defaults to empty", [] { Fixture f; f.cfg.initReturnConfigForTest(); assert(f.cfg.configMain[cfg::returnApp].valueStr.empty()); });
    test("default hotkey maps physical Back and Start", [] {
        Fixture f;
#ifdef _XBOX
        assert(f.joystick.inputs.mapperHotkeys.getSdlBtn(0, HK_MODIFIER) == 9);
        assert(f.joystick.inputs.mapperHotkeys.getSdlBtn(0, HK_EXIT_GAME) == 8);
#else
        assert(f.joystick.inputs.mapperHotkeys.getSdlBtn(0, HK_MODIFIER) == 6);
        assert(f.joystick.inputs.mapperHotkeys.getSdlBtn(0, HK_EXIT_GAME) == 7);
#endif
        f.joystick.inputs.btn_state[0][sdlBtnOf(JOY_BUTTON_START)] = true;
        assert(f.keys.procesarHotkeys(&f.joystick.inputs) == HK_MAX);
        f.joystick.inputs.btn_state[0][sdlBtnOf(JOY_BUTTON_SELECT)] = true;
        assert(f.keys.procesarHotkeys(&f.joystick.inputs) == HK_EXIT_GAME);
        assert(!f.joystick.inputs.btn_state[0][sdlBtnOf(JOY_BUTTON_START)]);
        ticks += 100; f.pressExit(); assert(f.keys.procesarHotkeys(&f.joystick.inputs) == HK_MAX);
    });
    test("remapped exit hotkey produces same action", [] {
        Fixture f; f.joystick.inputs.mapperHotkeys.setBtnFromSdl(0, 10, HK_MODIFIER);
        f.joystick.inputs.mapperHotkeys.setBtnFromSdl(0, 11, HK_EXIT_GAME); f.pressExit();
        assert(f.keys.procesarHotkeys(&f.joystick.inputs) == HK_EXIT_GAME);
    });
    test("unset target retains normal exit-game menu", [] { Fixture f; f.game.exitHotkeyForTest(HK_EXIT_GAME); assert(f.game.status == EMU_MENU && !f.game.returnAppRequested && f.game.romLoaded); noHandoff(); });
    test("achievements pause guard still precedes return request", [] { Fixture f; f.configure(); pauseAllowed = false; f.game.exitHotkeyForTest(HK_EXIT_GAME); assert(!f.game.returnAppRequested && f.game.status == EMU_STARTED && f.game.messages.size() == 1); noHandoff(); });
    test("configured exit hotkey defers handoff", [] {
        Fixture f; f.configure(); f.game.exitHotkeyForTest(HK_EXIT_GAME);
#ifdef _XBOX
        assert(f.game.returnAppRequested && f.game.status == EMU_STARTED);
#else
        assert(!f.game.returnAppRequested && f.game.status == EMU_MENU);
#endif
        noHandoff();
    });
    test("unset configuration adds no return menu entry", [] { Fixture f; f.menus.buildReturnEntryForTest(&f.cfg); assert(f.menus.root.opciones.empty()); });
    test("configured return menu invokes production callback", [] {
        Fixture f; f.configure(); f.menus.buildReturnEntryForTest(&f.cfg);
#ifdef _XBOX
        assert(f.menus.root.opciones.size() == 1); f.menus.root.opciones[0]->execute();
        assert(f.menus.status == RETURN_TO_APP);
        t_option_action action; processActions(gameMenu, action);
        assert(f.game.returnAppRequested && f.menus.status == NORMAL && f.game.running);
#else
        assert(f.menus.root.opciones.empty());
#endif
        noHandoff();
    });
    test("ordinary Exit Salvia does not request external app", [] {
        Fixture f; f.configure(); f.menus.salirEmulacion(&f.menus.status); t_option_action action;
        processActions(gameMenu, action); assert(!f.game.running && !f.game.returnAppRequested); noHandoff();
    });
    test("ordinary Return to game keeps core loaded", [] {
        Fixture f; f.game.status = EMU_MENU_OVERLAY; f.game.lastStatus = EMU_STARTED;
        f.menus.volverEmulacion(&f.menus.status); t_option_action action; processActions(gameMenu, action);
        assert(f.game.status == EMU_STARTED && f.game.romLoaded && !f.game.returnAppRequested); noHandoff();
    });
}

static void loopTests() {
#ifdef _XBOX
    const int routes[] = {INPUT_HOTKEY, INPUT_MENU};
    for (std::size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        const int route = routes[i];
        test(route == INPUT_HOTKEY ? "real hotkey through loop saves, unloads and launches once" : "real menu action through loop saves, unloads and launches once", [route] {
            Fixture f; f.configure(); f.pressExit(); f.menus.buildReturnEntryForTest(&f.cfg); inputMode = route;
            runGameLoop();
            assert(at("callback-end") < at("target-check"));
#ifndef NO_SRAM
            assert(at("file-close") < at("unload"));
            assert(readSave() == std::vector<unsigned char>({'N', 'E', 'W'}));
#endif
            assert(at("cheats-clear") < at("unload") && at("unload") < at("deinit"));
            assert(at("deinit") < at("clear-launch-data") && at("clear-launch-data") < at("launch"));
            assert(count("async-save") == 0 && count("launch") == 1 && count("unload") == 1);
            assert(!f.game.romLoaded && !f.game.returnAppRequested && f.game.status == EMU_MENU && f.game.lastStatus == EMU_MENU);
            assert(gameUpdates == 0 && f.game.messages.size() == 1 && f.menus.gameName.empty());
            assert(g_num_mem_descriptors == 0 && g_singleSampleFrames == 0 && !g_midiFastForwardCut);
            assert(action_postponed.cycles == -1 && action_postponed.action == SAVE_NONE);
        });
    }
    test("missing file leaves core loaded with no save or launch", [] { Fixture f; f.configure(); targetAttributes = INVALID_FILE_ATTRIBUTES; f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop(); noHandoff(); assert(f.game.romLoaded && count("ram-size") == 0 && f.game.messages.size() == 1); });
    test("directory target cannot trigger handoff", [] { Fixture f; f.configure(); targetAttributes = FILE_ATTRIBUTE_DIRECTORY; f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop(); noHandoff(); assert(f.game.romLoaded && count("ram-size") == 0); });
    test("invalid path never reaches target API", [] { Fixture f; f.cfg.configMain[cfg::returnApp].valueStr = "../default.xex"; f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop(); noHandoff(); assert(count("target-check") == 0); });
    test("save timeout cancels whole handoff and keeps game resumable", [] {
        Fixture f; f.configure(); f.queue(-1); f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop(); noHandoff();
        assert(f.game.romLoaded && f.game.lastStatus == EMU_STARTED && f.game.messages.size() == 1 && !f.game.returnAppRequested);
    });
    test("queued old snapshot cannot overwrite final SRAM during handoff", [] {
        Fixture f; f.configure(); f.queue(3); f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop();
        assert(at("worker-complete") < at("unload"));
#ifndef NO_SRAM
        assert(readSave() == std::vector<unsigned char>({'N', 'E', 'W'}));
#endif
        assert(count("launch") == 1 && count("async-save") == 0);
    });
#ifndef NO_SRAM
    test("final SRAM write failure cancels whole handoff", [] { Fixture f; f.configure(); failure = FAIL_FLUSH; f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop(); noHandoff(); assert(f.game.romLoaded && f.game.messages.size() == 1); });
    test("save failure can be retried with current RAM", [] {
        Fixture f; f.configure(); failure = FAIL_WRITE; f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop();
        noHandoff(); assert(f.game.romLoaded);
        failure = NO_FAILURE; ram.assign({'L', 'A', 'T', 'E'}); ticks += 300;
        frames = 0; f.game.running = true; f.pressExit(); runGameLoop();
        assert(count("launch") == 1 && readSave() == std::vector<unsigned char>({'L', 'A', 'T', 'E'}));
    });
#endif
    test("menu can return with no loaded core", [] {
        Fixture f; f.configure(); f.game.romLoaded = false; unloaded = true;
        f.game.status = f.game.lastStatus = EMU_MENU; f.menus.buildReturnEntryForTest(&f.cfg);
        inputMode = INPUT_MENU; runGameLoop();
        assert(count("unload") == 0 && count("deinit") == 0 && count("ram-size") == 0 && count("launch") == 1);
    });
    test("failed launch-data clearing does not launch", [] {
        Fixture f; f.configure(); clearDataResult = 1; f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop();
        assert(count("clear-launch-data") == 1 && count("launch") == 0 && !f.game.romLoaded && f.game.messages.size() == 1);
    });
    test("simulated nonreturning handoff has no failure UI", [] {
        Fixture f; f.configure(); replaceProcess = true; f.pressExit(); inputMode = INPUT_HOTKEY;
        bool replaced = false; try { runGameLoop(); } catch (const ProcessReplaced&) { replaced = true; }
        assert(replaced && count("launch") == 1 && f.game.messages.empty());
    });
    test("pending return request is skipped after ordinary quit", [] { Fixture f; f.configure(); inputMode = INPUT_QUIT; runGameLoop(); noHandoff(); });
    test("already unloaded core is not unloaded a second time", [] { Fixture f; closeGame(true); events.clear(); closeGame(true); assert(events.empty()); });
    test("production teardown orders audio, disc, cheats and memory cleanup", [] {
        Fixture f; audio_opened = 1; f.music.active = true; g_music = &f.music;
        disk_control.get_num_images = []() -> unsigned { return 2; };
        disk_control.get_image_index = []() -> unsigned { return 1; };
        closeGame(true);
        assert(at("audio-pause") < at("audio-clear") && at("disc-save") < at("unload"));
        assert(at("cheats-clear") < at("unload") && at("deinit") < at("midi-close"));
        assert(at("midi-close") < at("audio-resume") && g_currentRompath.empty());
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(g_mem_descriptors);
        for (std::size_t i = 0; i < sizeof(g_mem_descriptors); ++i) assert(bytes[i] == 0);
    });
    test("normal close path keeps its ordinary SRAM request", [] {
        Fixture f; closeGame(); assert(count("unload") == 1 && count("deinit") == 1);
#ifndef NO_SRAM
        assert(count("async-save") == 1);
#else
        assert(count("async-save") == 0);
#endif
    });
#else
    test("Windows configured hotkey returns to Salvia without handoff", [] { Fixture f; f.configure(); f.pressExit(); inputMode = INPUT_HOTKEY; runGameLoop(); assert(f.game.status == EMU_MENU && f.game.romLoaded && !f.game.returnAppRequested); noHandoff(); });
#endif
}

int main(int argc, char** argv) {
    assert(argc == 2); scratch = argv[1];
    drainTests(); inputTests(); loopTests();
    std::cout << passed << " production-body cases passed\n";
}
