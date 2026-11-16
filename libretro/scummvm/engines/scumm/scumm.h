/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SCUMM_SCUMM_H
#define SCUMM_SCUMM_H

#include "engines/engine.h"

#include "common/endian.h"
#include "common/events.h"
#include "common/file.h"
#include "common/savefile.h"
#include "common/keyboard.h"
#include "common/mutex.h"
#include "common/random.h"
#include "common/rect.h"
#include "common/rendermode.h"
#include "common/str.h"
#include "common/textconsole.h"
#include "common/text-to-speech.h"
#include "graphics/surface.h"
#include "graphics/sjis.h"
#include "graphics/palette.h"

#include "scumm/file.h"
#include "scumm/gfx.h"
#include "scumm/detection.h"
#include "scumm/script.h"
#include "scumm/serializer.h"

#ifdef __DS__
/* This disables the dual layer mode which is used in FM-Towns versions
 * of SCUMM games and which emulates the behavior of the original code.
 * The only purpose is code size reduction for certain backends.
 * SCUMM 3 (FM-Towns) games will run in English in normal (DOS VGA) mode,
 * which should work just fine in most situations. Some glitches might
 * occur. Japanese mode and SCUMM 5 FM-Towns games will not work without
 * dual layer (and 16 bit color) support.
 */
#define DISABLE_TOWNS_DUAL_LAYER_MODE
#endif

namespace GUI {
class Dialog;
}
using GUI::Dialog;
namespace Common {
class SeekableReadStream;
class WriteStream;
class SeekableWriteStream;
}
namespace Graphics {
class FontSJIS;
}

/**
 * This is the namespace of the SCUMM engine.
 *
 * Status of this engine:
 * Complete support for all SCUMM based LucasArts adventures.
 * Complete support for many Humongous Entertainment games,
 * but for some of the newer ones, this is still work in progress.
 *
 * Games using this engine:
 * - Classic 2D LucasArts adventures
 * - numerous Humongous Entertainment games
 */
namespace Scumm {

class Actor;
class BaseCostumeLoader;
class BaseCostumeRenderer;
class BaseScummFile;
class CharsetRenderer;
class IMuse;
class IMuseDigital;
class MacGui;
class MusicEngine;
class Player_Towns;
class ScummEngine;
class ScummDebugger;
class Sound;
class Localizer;
class GlyphRenderer_v7;

struct Box;
struct BoxCoords;
struct FindObjectInRoom;

// Use g_scumm from error() ONLY
extern ScummEngine *g_scumm;

/* System Wide Constants */
enum {
	NUM_SENTENCE = 6,
	NUM_SHADOW_PALETTE = 8
};

/* SCUMM Debug Channels */
void debugC(int level, MSVC_PRINTF const char *s, ...) GCC_PRINTF(2, 3);

struct VerbSlot;
struct ObjectData;

enum {
	/**
	 * Lighting flag that indicates whether the normal palette, or the 'dark'
	 * palette shall be used to draw actors.
	 * Apparently only used in very old games (so far only NESCostumeRenderer
	 * checks it).
	 */
	LIGHTMODE_actor_use_base_palette	= 1 << 0,

	/**
	 * Lighting flag that indicates whether the room is currently lit. Normally
	 * always on. Used for rooms in which the light can be switched "off".
	 */
	LIGHTMODE_room_lights_on			= 1 << 1,

	/**
	 * Lighting flag that indicates whether a flashlight like device is active.
	 * Used in Loom (flashlight follows the actor) and Indy 3 (flashlight
	 * follows the mouse). Only has any effect if the room lights are off.
	 */
	LIGHTMODE_flashlight_on				= 1 << 2,

	/**
	 * Lighting flag that indicates whether actors are to be drawn with their
	 * own custom palette, or using a fixed 'dark' palette. This is the
	 * modern successor of LIGHTMODE_actor_use_base_palette.
	 * Note: It is tempting to 'merge' these two flags, but since flags can
	 * check their values, this is probably not a good idea.
	 */
	LIGHTMODE_actor_use_colors	= 1 << 3
	//
};

enum {
	MBS_LEFT_CLICK = 0x8000,
	MBS_RIGHT_CLICK = 0x4000,
	MBS_MOUSE_MASK = (MBS_LEFT_CLICK | MBS_RIGHT_CLICK),
	MBS_MAX_KEY	= 0x0200
};

struct SentenceTab {
	byte verb;
	byte preposition;
	uint16 objectA;
	uint16 objectB;
	uint8 freezeCount;
};

struct StringSlot {
	int16 xpos;
	int16 ypos;
	int16 right;
	int16 height;
	byte color;
	byte charset;
	bool center;
	bool overhead;
	bool no_talk_anim;
	bool wrapping;
};

struct StringTab : StringSlot {
	// The 'default' values for this string slot. This is used so that the
	// string slot can temporarily be set to different values, and then be
	// easily reset to a previously set default.
	StringSlot _default;

	void saveDefault() {
		StringSlot &s = *this;
		_default = s;
	}

	void loadDefault() {
		StringSlot &s = *this;
		s = _default;
	}
};

struct ScummEngine_v0_Delays {
	bool _screenScroll;
	uint _objectRedrawCount;
	uint _objectStripRedrawCount;
	uint _actorRedrawCount;
	uint _actorLimbRedrawDrawCount;

};

enum WhereIsObject {
	WIO_NOT_FOUND = -1,
	WIO_INVENTORY = 0,
	WIO_ROOM = 1,
	WIO_GLOBAL = 2,
	WIO_LOCAL = 3,
	WIO_FLOBJECT = 4
};

struct SaveStateMetaInfos {
	uint32 date;
	uint16 time;
	uint32 playtime;
};

enum UserStates {
	USERSTATE_SET_FREEZE      = 0x01,   // freeze scripts if USERSTATE_FREEZE_ON is set, unfreeze otherwise
	USERSTATE_SET_CURSOR      = 0x02,   // shows cursor if USERSTATE_CURSOR_ON is set, hides it otherwise
	USERSTATE_SET_IFACE       = 0x04,   // change user-interface (sentence-line, inventory, verb-area)
	USERSTATE_FREEZE_ON       = 0x08,   // only interpreted if USERSTATE_SET_FREEZE is set
	USERSTATE_CURSOR_ON       = 0x10,   // only interpreted if USERSTATE_SET_CURSOR is set
	USERSTATE_IFACE_SENTENCE  = 0x20,   // only interpreted if USERSTATE_SET_IFACE is set
	USERSTATE_IFACE_INVENTORY = 0x40,   // only interpreted if USERSTATE_SET_IFACE is set
	USERSTATE_IFACE_VERBS     = 0x80    // only interpreted if USERSTATE_SET_IFACE is set
};

#define USERSTATE_IFACE_ALL (USERSTATE_IFACE_SENTENCE | USERSTATE_IFACE_INVENTORY | USERSTATE_IFACE_VERBS)

/**
 * A list of resource types.
 * WARNING: Do not change the order of these, as the savegame format relies
 * on it; any change made here will break savegame compatibility!
 */
enum ResType {
	rtInvalid = 0,
	rtFirst = 1,
	rtRoom = 1,
	rtScript = 2,
	rtCostume = 3,
	rtSound = 4,
	rtInventory = 5,
	rtCharset = 6,
	rtString = 7,
	rtVerb = 8,
	rtActorName = 9,
	rtBuffer = 10,
	rtScaleTable = 11,
	rtTemp = 12,
	rtFlObject = 13,
	rtMatrix = 14,
	rtBox = 15,
	rtObjectName = 16,
	rtRoomScripts = 17,
	rtRoomImage = 18,
	rtImage = 19,
	rtTalkie = 20,
	rtSpoolBuffer = 21,
	rtLast = 21
};

typedef uint16 ResId;

class ResourceManager;

/**
 * DOS Programmable Interval Timer constants.
 *
 * The SCUMM engine (v1-v7, DOS) timer ticks are based on the jiffy unit (roughly 60Hz).
 * Well, if we want to be pedantic about it, it operates on quarter jiffies (240Hz),
 * a rate at which several screen effects are updated; but still, this value is divided
 * by 4 in the main game loop in order for it to operate on whole jiffies.
 * In order to obtain this behavior, the PIT is programmed to operate at roughly 240Hz,
 * though these timings change from version to version (or game by game, for v6).
 *
 * Glossary:
 * - Base frequency: this is the frequency at which the Intel 8253/54 PIT
 *                   operates, namely obtained with the formula 105/88, which
 *                   yields 1.193181818... MHz. We are storing it in Hz;
 *
 * - Divisor:        the base frequency in DOS is not used as-is, but it is instead
 *                   divided by a customizable divisor which can range between
 *                   0 and (2^16-1), where 0 is a shortcut for 2^16. This operation
 *                   yields the custom frequency at which our custom interrupt
 *                   gets called (hence "Programmable");
 *
 * - Orchestrator:   starting from SCUMM v5, games started using iMUSE, and apparently
 *                   needed a more precise timing handling; this led to the introduction
 *                   of a main orchestrator timer (which then handled the execution of
                     other sub-timers), whose divisor (4096) was set up in the IMS
 *                   drivers up until v7, in which the divisor (3977) was set up in the
 *                   executable as a part of the INSANE orchestration;
 *
 * - Sub-timer:      custom made timers, operating under an orchestrator; in the macros
 *                   below, "INC" refers to the increment of an accumulator which gets
 *                   updated at each iteration of the orchestrator interrupt; "THRESH"
 *                   refers to a threshold value of the aforementioned accumulator,
 *                   beyond which the accumulator is decremented by the threshold value,
 *                   and the interrupt of the sub-timer gets executed (e.g. the values
 *                   below mainly refer to the interrupt which increments the SCUMM
 *                   quarter frame counter.
 *
 * All the values below are presented as doubles, so to safely yield fractional results.
 */

#define PIT_BASE_FREQUENCY             1193182.0 // In Hz
#define PIT_V1_DIVISOR                 65536.0
#define PIT_V2_4_DIVISOR               5041.0
#define PIT_V5_6_ORCHESTRATOR_DIVISOR  4096.0
#define PIT_V5_6_SUBTIMER_INC          3433.0
#define PIT_V5_SUBTIMER_THRESH         4167.0
#define PIT_V6_SAMNMAX_SUBTIMER_THRESH 4167.0
#define PIT_V6_DOTT_SUBTIMER_THRESH    4237.0
#define PIT_V7_ORCHESTRATOR_DIVISOR    3977.0
#define PIT_V7_SUBTIMER_INC            3977.0
#define PIT_V7_SUBTIMER_THRESH         4971.0

#define PIT_HE_PUTT_PUTT_DIVISOR       9362.0
#define PIT_HE_FATTY_BEAR_DIVISOR      21845.0

#define LOOM_STEAM_CDDA_RATE           240.0

/**
 * Amiga timing constants.
 *
 * Amiga versions of SCUMM games update the game timer at every
 * V-Blank interrupt, incrementing it by 4 each time (which means
 * a full frame/jiffie). The shake timer is instead updated every
 * other V-Blank interrupt, so 8 quarter frames (2 frames/jiffies)
 * at a time.
 *
 * The base rate is 50Hz for PAL systems and 60Hz for NTSC systems.
 * We're converting it in a quarter frame frequency.
 */

#define AMIGA_NTSC_VBLANK_RATE 240.0
#define AMIGA_PAL_VBLANK_RATE  200.0

/**
 * C64 timing constants
 *
 * While we don't detect from the game if it's for PAL or NTSC, we
 * do allow the user to specify by using an engine setting. The music
 * plays slower on PAL systems, so we have to adjust the frame rate
 * from 60Hz for NTSC to 50Hz for PAL.
 */

#define C64_NTSC_VBLANK_RATE 240.0
#define C64_PAL_VBLANK_RATE  200.0

/**
 * Game saving/loading outcome codes
 */

#define GAME_PROPER_SAVE 201
#define GAME_FAILED_SAVE 202
#define GAME_PROPER_LOAD 203
#define GAME_FAILED_LOAD 204

/**
 * GUI defines and enums.
 */

#define GUI_PAGE_MAIN         0
#define GUI_PAGE_SAVE         1
#define GUI_PAGE_LOAD         2
#define GUI_PAGE_RESTART      3 // Sega CD
#define GUI_PAGE_CODE_CONFIRM 4 // Sega CD
#define GUI_PAGE_INVALID_CODE 5 // Sega CD

#define GUI_CTRL_FIRST_SG               1
#define GUI_CTRL_LAST_SG                9
#define GUI_CTRL_SAVE_BUTTON            10
#define GUI_CTRL_LOAD_BUTTON            11
#define GUI_CTRL_PLAY_BUTTON            12
#define GUI_CTRL_QUIT_BUTTON            13
#define GUI_CTRL_OK_BUTTON              14
#define GUI_CTRL_CANCEL_BUTTON          15
#define GUI_CTRL_ARROW_UP_BUTTON        16
#define GUI_CTRL_ARROW_DOWN_BUTTON      17
#define GUI_CTRL_PATH_BUTTON            18
#define GUI_CTRL_MUSIC_SLIDER           19
#define GUI_CTRL_SPEECH_SLIDER          20
#define GUI_CTRL_SFX_SLIDER             21
#define GUI_CTRL_TEXT_SPEED_SLIDER      22
#define GUI_CTRL_DISPLAY_TEXT_CHECKBOX  23
#define GUI_CTRL_SPOOLED_MUSIC_CHECKBOX 24
#define GUI_CTRL_OUTER_BOX              26
#define GUI_CTRL_INNER_BOX              27

// Sega CD
#define GUI_CTRL_NUMPAD_1           1
#define GUI_CTRL_NUMPAD_2           2
#define GUI_CTRL_NUMPAD_3           3
#define GUI_CTRL_NUMPAD_4           4
#define GUI_CTRL_NUMPAD_5           5
#define GUI_CTRL_NUMPAD_6           6
#define GUI_CTRL_NUMPAD_7           7
#define GUI_CTRL_NUMPAD_8           8
#define GUI_CTRL_NUMPAD_9           9
#define GUI_CTRL_NUMPAD_0           10
#define GUI_CTRL_RESTART_BUTTON     13
#define GUI_CTRL_ARROW_LEFT_BUTTON  16
#define GUI_CTRL_ARROW_RIGHT_BUTTON 17
#define GUI_CTRL_NUMPAD_BACK        23

enum GUIString {
	gsPause = 0,
	gsVersion = 1,
	gsTextSpeedSlider = 2,
	gsRestart = 3,
	gsQuitPrompt = 4,
	gsSave = 5,
	gsLoad = 6,
	gsPlay = 7,
	gsCancel = 8,
	gsQuit = 9,
	gsOK = 10,
	gsMustName = 11,
	gsGameNotSaved = 12,
	gsGameNotLoaded = 13,
	gsSaving = 14,
	gsLoading = 15,
	gsNamePrompt = 16,
	gsSelectLoadPrompt = 17,
	gsReplacePrompt = 18,
	gsYes = 20,
	gsNo = 21,
	gsIMuseBuffer = 22,
	gsVoiceAndText = 23,
	gsTextDisplayOnly = 24,
	gsVoiceOnly = 25,
	gsYesKey = 26,
	gsMusicVolumeSlider = 27,
	gsVoiceVolumeSlider = 28,
	gsSfxVolumeSlider = 29,
	gsHeap = 30,
	gsSavePath = 31,
	gsTitle = 32,
	gsDisabled = 33,
	gsMusic = 34,
	gsVoice = 35,
	gsSfx = 36,
	gsTextSpeed = 37,
	gsDisplayText = 38,
	gsSpooledMusic = 39,
	gsInsertSaveDisk = 40,
	gsSnapOn = 41,
	gsSnapOff = 42,
	gsRecalJoystick = 43,
	gsMouseMode = 44,
	gsMouseOn = 45,
	gsMouseOff = 46,
	gsJoystickOn = 47,
	gsJoystickOff = 48,
	gsSoundsOn = 49,
	gsSoundsOff = 50,
	gsVGAMode = 51,
	gsEGAMode = 52,
	gsCGAMode = 53,
	gsHerculesMode = 54,
	gsTandyMode = 55,
	gsCurrentPasscode = 56,
	gsEnterPasscode = 57,
	gsConfirmPasscode = 58,
	gsInvalidPasscode = 59,
	gsSlowFast = 60,
	gsRestartGame = 61,
	gsHeapExt = 62,
};

struct InternalGUIControl {
	int relativeCenterX;
	int relativeCenterY;
	int xPos;
	int yPos;
	int normalFillColor;
	int topLineColor;
	int bottomLineColor;
	int leftLineColor;
	int rightLineColor;
	int normalTextColor;
	int highlightedTextColor;
	int highlightedFillColor;
	bool centerText;
	Common::String label;
#ifdef USE_TTS
	Common::String alternateTTSLabel;
#endif
	bool doubleLinesFlag;
};

enum ScummAction {
	kScummActionNone,
	kScummActionInsaneDownLeft,
	kScummActionInsaneDown,
	kScummActionInsaneDownRight,
	kScummActionInsaneLeft,
	kScummActionInsaneRight,
	kScummActionInsaneUpLeft,
	kScummActionInsaneUp,
	kScummActionInsaneUpRight,
	kScummActionInsaneAttack,
	kScummActionInsaneSwitch,
	kScummActionInsaneCheat,
	kScummActionInsaneBack,
	kScummActionInsaneSkip,

	kScummActionCount
};

enum ScummBackendAction {
	kScummBackendActionRebel1AxisUp = 11000,
	kScummBackendActionRebel1AxisDown,
	kScummBackendActionRebel1AxisLeft,
	kScummBackendActionRebel1AxisRight,
	kScummBackendActionRebel2AxisUp,
	kScummBackendActionRebel2AxisDown,
	kScummBackendActionRebel2AxisLeft,
	kScummBackendActionRebel2AxisRight
};

extern const char *const insaneKeymapId;

/**
 * Base class for all SCUMM engines.
 */
class ScummEngine : public Engine, public Common::Serializable {
	friend class ScummDebugger;
	friend class CharsetRenderer;
	friend class CharsetRendererClassic;
	friend class CharsetRendererTownsClassic;
	friend class ResourceManager;
	friend class MacGuiImpl;
	friend class MacIndy3Gui;
	friend class MacLoomGui;
	friend class MacV5Gui;
	friend class MacV6Gui;
	friend class LogicHEBasketball;
	friend class ScummEditor;

public:
	/* Put often used variables at the top.
	 * That results in a shorter form of the opcode
	 * on some architectures. */
	IMuse *_imuse;
	IMuseDigital *_imuseDigital;
	MusicEngine *_musicEngine;
	Player_Towns *_townsPlayer;
	Sound *_sound;

	VerbSlot *_verbs;
	ObjectData *_objs;

	// Core variables
	GameSettings _game;
	uint8 _gameMD5[16];

	/** Random number generator */
	Common::RandomSource _rnd;

	/** Graphics manager */
	Gdi *_gdi;

	/** Central resource data. */
	ResourceManager *_res;
	int _insideCreateResource; // Counter for HE sound

	bool _useOriginalGUI;
	bool _enableAudioOverride;
	bool _enableCOMISong;
	bool _isAmigaPALSystem;
	bool _isC64PALSystem;
	bool _quitFromScriptCmd;
	bool _isHE995;
	bool _enableHECompetitiveOnlineMods;

	Common::Keymap *_insaneKeymap;

protected:
	VirtualMachineState vm;

	bool _needsSoundUnpause;

public:
	// Constructor / Destructor
	ScummEngine(OSystem *syst, const DetectorResult &dr);
	~ScummEngine();

	// Engine APIs
	Common::Error init();
	Common::Error go();
	Common::Error run() override {
		Common::Error err;
		err = init();
		if (err.getCode() != Common::kNoError)
			return err;
		return go();
	}

	void errorString(const char *buf_input, char *buf_output, int buf_output_size) override;
	bool hasFeature(EngineFeature f) const override;
	bool gameSupportsQuitDialogOverride() const;
	void syncSoundSettings() override;

	Common::Error loadGameState(int slot) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::Error saveGameState(int slot, const Common::String &desc, bool isAutosave = false) override;
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::String getSaveStateName(int slot) const override { return makeSavegameName(slot, false); }

	void pauseEngineIntern(bool pause) override;

protected:
	virtual void setupScumm(const Common::Path &macResourceFile);
	virtual void resetScumm();

	virtual void setupScummVars();
	virtual void resetScummVars();
	void setVideoModeVarToCurrentConfig();
	void setSoundCardVarToCurrentConfig();

	void setupCharsetRenderer(const Common::Path &macFontFile);
	void setupCostumeRenderer();

	virtual void loadLanguageBundle();
	void loadCJKFont();
	void loadKorFont();
	void setupMusic(int midi);
	void setTalkSpeed(int talkspeed);
	int getTalkSpeed();

	// Scumm main loop & helper functions.
	virtual void scummLoop(int delta);
	virtual void scummLoop_updateScummVars();
	virtual void scummLoop_handleSaveLoad();
	virtual void scummLoop_handleDrawing();
	virtual void scummLoop_handleActors() = 0;
	virtual void scummLoop_handleEffects();
	virtual void scummLoop_handleSound();

	virtual void runBootscript();

	virtual void terminateSaveMenuScript() {};

	// Event handling
public:
	void beginTextInput();
	void endTextInput();

	void parseEvents();	// Used by IMuseDigital::startSound
protected:
	virtual void parseEvent(Common::Event event);

	void waitForTimer(int quarterFrames, bool freezeMacGui = false);
	uint32 _lastWaitTime;

	void setTimerAndShakeFrequency();

	/**
	 * Represents fractional milliseconds by decomposing the passed
	 * value into integral and fractional parts, then incrementing the
	 * integer part as needed on subsequent function calls.
	 */
	uint32 getIntegralTime(double fMsecs);
	double _msecFractParts;

	virtual void processInput();
	virtual void processKeyboard(Common::KeyState lastKeyHit);
	virtual void clearClickedStatus();

	// Cursor/palette
	virtual void updateCursor();
	virtual void animateCursor() {}
	virtual void updatePalette();
	virtual void setDefaultCursor() {};
	virtual void setCursorTransparency(int a) {};
	virtual void resetCursors() {}
	virtual void setCursorHotspot(int x, int y) {}
	virtual void setCursorFromBuffer(const byte *ptr, int width, int height, int pitch, bool preventScale = false) {}

public:
	void pauseGame();
	void restart();
	bool isUsingOriginalGUI() const;
	bool isMessageBannerActive(); // For Indy4 Jap character shadows

	bool _isIndy4Jap;

protected:
	Dialog *_pauseDialog;
	Dialog *_messageDialog;
	Dialog *_versionDialog;

	void confirmExitDialog();
	void confirmRestartDialog();
	void pauseDialog();
	void messageDialog(const Common::U32String &message);
	void versionDialog();

	// Original GUI
	int32 _bannerColors[50]; // Colors for the original GUI
	byte *_bannerMem;
	uint32 _bannerMemSize;
	int _bannerSaveYStart;

	bool _messageBannerActive;
	bool _comiQuitMenuIsOpen;
	bool _closeBannerAndQueryQuitFlag;

	// The followings are needed for MI1 FM-Towns
	byte *_textSurfBannerMem;
	uint32 _textSurfBannerMemSize;

	InternalGUIControl _internalGUIControls[30];

	// Special GUI strings
	static const char _emptyMsg[1];
	static const char _uncheckedBox[2];
	static const char _checkedBox[2];
	static const char _arrowUp[2];
	static const char _arrowDown[2];
	static const char _arrowLeft[2];
	static const char _arrowRight[2];

	Common::StringArray _savegameNames;
	int _menuPage;
	int _mainMenuSavegameLabel;
	int _curDisplayedSaveSlotPage;
	int _firstSaveStateOfList; // For LOOM VGA
	bool _mainMenuIsActive;
	bool _quitByGUIPrompt;
	char _mainMenuMusicSlider[17];
	char _mainMenuSpeechSlider[17];
	char _mainMenuSfxSlider[17];
	char _mainMenuTextSpeedSlider[17];
	char _mainMenuSegaCDPasscode[5];
	int _spooledMusicIsToBeEnabled;
	int _saveScriptParam;
	int _guiCursorAnimCounter;
	int _v5VoiceMode;

	// Fake flags just for sub v5 GUIs
	int _internalSpeakerSoundsAreOn;
	int _guiMouseFlag;
	int _guiJoystickFlag;

	bool _mixerMutedByGUI;

	Graphics::Surface _savegameThumbnail;
	byte *_tempTextSurface;
	byte *_tempMainSurface;
	byte *_tempVerbSurface;
	bool _postGUICharMask;

	// Saved cursor pre and post GUI
	byte *_curGrabbedCursor;
	int8 _oldCursorState;
	int _curCursorState;
	int _curCursorWidth;
	int _curCursorHeight;
	int _curCursorHotspotX;
	int _curCursorHotspotY;

	virtual void setSnailCursor() {}

	void initBanners();
	Common::KeyState showBannerAndPause(int bannerId, int32 waitTime, const char *msg, ...);
	bool showBannerAndPauseForTextInput(int bannerId, const char *prompt, Common::String &input, uint maxLength = 255);
	Common::KeyState showOldStyleBannerAndPause(const char *msg, int color, int32 waitTime);
	Common::KeyState printMessageAndPause(const char *msg, int color, int32 waitTime, bool drawOnSentenceLine);

	void clearBanner();
	void setBannerColors(int bannerId, byte r, byte g, byte b);
	virtual int getBannerColor(int bannerId);
	void setUpInternalGUIControl(int id, int normalFillColor, int normalTextColor,
								 int topLineColor, int bottomLineColor, int leftLineColor, int rightLineColor,
								 int highlightedTextColor, int highlightedFillColor,
								 int anchorPointX, int anchorPointY, int x, int y, const char *label, bool centerFlag, bool unknownFlag);
	void drawInternalGUIControl(int id, bool highlightColor);
	int getInternalGUIControlFromCoordinates(int x, int y);
	virtual bool isSmushActive() { return false; }
	virtual bool isInsaneActive() { return false; }

	virtual void queryQuit(bool returnToLauncher);
	virtual void queryRestart();
	virtual const char *getGUIString(int stringId);
	void waitForBannerInput(int32 waitTime, Common::KeyState &ks, bool &leftBtnClicked, bool &rightBtnClicked, bool handleMouseWheel = false);
	virtual int getGUIStringHeight(const char *str);
	virtual int getGUIStringWidth(const char *str);
	virtual void drawGUIText(const char *buttonString, Common::Rect *clipRect, int textXPos, int textYPos, int textColor, bool centerFlag);
	void getSliderString(int stringId, int value, char *sliderString, int size);
	virtual int getMusicVolume();
	virtual int getSpeechVolume();
	virtual int getSFXVolume();
	virtual void setMusicVolume(int volume);
	virtual void setSpeechVolume(int volume);
	virtual void setSFXVolume(int volume);
	virtual void toggleVoiceMode();
	virtual void handleLoadDuringSmush() {}
	virtual void setSkipVideo(int value) {}

	void showMainMenu();
	virtual void setUpMainMenuControls();
	void setUpMainMenuControlsSegaCD();
	void setUpMainMenuControlsIndy4Jap();
	void drawMainMenuControls();
	void drawMainMenuControlsSegaCD();
	void updateMainMenuControls();
	void updateMainMenuControlsSegaCD();
	void drawMainMenuTitle(const char *title);
	bool executeMainMenuOperation(int op, int mouseX, int mouseY, bool &hasLoadedState);
	bool executeMainMenuOperationSegaCD(int op, int mouseX, int mouseY, bool &hasLoadedState);
	bool shouldHighlightLabelAndWait(int clickedControl);
	void fillSavegameLabels();
	bool canWriteGame(int slotId);
	bool userWriteLabelRoutine(Common::KeyState &ks, bool &leftMsClicked, bool &rightMsClicked);
	void saveCursorPreMenu();
	void restoreCursorPostMenu();
	void saveSurfacesPreGUI();
	void restoreSurfacesPostGUI();
	void showDraftsInventory();
	void setUpDraftsInventory();
	void drawDraftsInventory();

public:
	char displayMessage(MSVC_PRINTF const char *message, ...) GCC_PRINTF(2, 3);
	bool displayMessageYesNo(MSVC_PRINTF const char *message, ...) GCC_PRINTF(2, 3);
	bool displayMessageOKQuit(MSVC_PRINTF const char *message, ...) GCC_PRINTF(2, 3);

protected:
	byte _fastMode;

	byte _numActors;
	Actor **_actors;	// Has _numActors elements
	Actor **_sortedActors;

	byte *_arraySlot;
	uint16 *_inventory;
	uint16 *_newNames;
public:
	// VAR is a wrapper around scummVar, which attempts to include additional
	// useful information should an illegal var access be detected.
	#define VAR(x)	scummVar(x, #x, __FILE__, __LINE__)
	int32& scummVar(byte var, const char *varName, const char *file, int line) {
		if (var == 0xFF) {
			error("Illegal access to variable %s in file %s, line %d", varName, file, line);
		}
		return _scummVars[var];
	}
	int32 scummVar(byte var, const char *varName, const char *file, int line) const {
		if (var == 0xFF) {
			error("Illegal access to variable %s in file %s, line %d", varName, file, line);
		}
		return _scummVars[var];
	}

	bool applyMi2NiDemoOverride();
	struct Playback {
	Playback() : _loaded(false), _attempted(false), _active(false), _mi2DemoVarsApplied(false), _streamOff(0), _streamBytes(0), _nextIndex(0), _lastRoom(-1), _hasPrevMbs(false), _prevMbs(0), _firstInteractIndex(0), _firstInteractValid(false), _curX(0), _curY(0), _pendingLUp(false), _pendingRUp(false), _sputmCmdActive(false), _sputmCmdEnterCount(0) {}

		struct FrameEvent {
	FrameEvent() : hasPos(false), x(0), y(0), mbs(0), key(0) {}

			bool hasPos;
			uint16 x;
			uint16 y;

			uint16 mbs;
			uint16 key;
		};

		static void parseStream(const Common::Array<byte> &stream, Common::Array<FrameEvent> &outEvents);

		bool _loaded;
		bool _attempted;
		bool _active;

		bool _mi2DemoVarsApplied;

		Common::Array<FrameEvent> _events;

		uint32 _streamOff;
		uint32 _streamBytes;

		uint32 _nextIndex;
		int _lastRoom;

		bool _hasPrevMbs;
		uint16 _prevMbs;
		uint32 _firstInteractIndex;
		bool _firstInteractValid;

		int16 _curX;
		int16 _curY;
		bool _pendingLUp;
		bool _pendingRUp;

		bool _sputmCmdActive;
		byte _sputmCmdEnterCount;
		Common::String _sputmCmdBuf;
		Common::String _name;

		void reset();
		bool tryLoadPlayback(ScummEngine *engine, const Common::Path &path = Common::Path("demo.rec"));
		bool startPlayback(ScummEngine *engine);
		void playbackPump(ScummEngine *engine);

		//MI2 DOS NI Demo specific playback helpers
		void mi2DemoArmPlaybackByRoom(ScummEngine *engine);
		void mi2DemoPlaybackJumpRoom(ScummEngine *engine, int room);
		bool handleMi2NIDemoSputmDebugKey(ScummEngine *engine, uint16 rawKey);
	};

	Playback _playback;

protected:
	int16 _varwatch;
	int32 *_roomVars;
	int32 *_scummVars;
	byte *_bitVars;

	/* Global resource tables */
	int _numVariables;
	int _numBitVariables;
	int _numLocalObjects;
	int _numGlobalObjects;
	int _numArray;
	int _numVerbs;
	int _numFlObject;
	int _numInventory;
	int _numNewNames;
	int _numGlobalScripts;
	int _numRoomVariables;
	int _numPalettes;
	int _numSprites;
	int _numTalkies;
	int _numWindows;
	int _HEHeapSize;

public:
	int _numLocalScripts, _numImages, _numRooms, _numScripts, _numSounds;	// Used by HE games
	int _numCostumes;	// FIXME - should be protected, used by Actor::remapActorPalette
	int32 _numCharsets;	// FIXME - should be protected, used by CharsetRenderer

	BaseCostumeLoader *_costumeLoader;
	BaseCostumeRenderer *_costumeRenderer;

	int _NESCostumeSet;
	void NES_loadCostumeSet(int n);
	byte *_NEScostdesc, *_NEScostlens, *_NEScostoffs, *_NEScostdata;
	byte _NESPatTable[2][4096];
	byte _NESPalette[2][16];
	byte _NESBaseTiles;

	int _NESStartStrip;

protected:
	int _curPalIndex;

public:
	byte _currentRoom;	// FIXME - should be protected but Actor::isInCurrentRoom uses it
	int _roomResource;  // FIXME - should be protected but Sound::pauseSounds uses it
	bool _egoPositioned;	// Used by Actor::putActor, hence public

	FilenamePattern _filenamePattern;

	virtual Common::Path generateFilename(const int room) const;

protected:
	Common::KeyState _keyPressed;
	bool _keyDownMap[512]; // FIXME - 512 is a guess. it's max(kbd.ascii)
	bool _actionMap[kScummActionCount];

	Common::Point _mouse;
	Common::Point _virtualMouse;

	uint16 _mouseAndKeyboardStat;
	byte _leftBtnPressed, _rightBtnPressed;

	int _mouseWheelFlag; // For original save/load dialog only

	bool _setupIsComplete;

	/**
	 * Last time runInputScript was run (measured in terms of OSystem::getMillis()).
	 * This is currently only used for Indy3 mac to detect "double clicks".
	 */
	uint32 _lastInputScriptTime;

	/** The bootparam, to be passed to the script 1, the bootscript. */
	int _bootParam;

	// Various options useful for debugging
	bool _dumpScripts;
	bool _hexdumpScripts;
	bool _showStack;
	bool _debugMode;

	// Save/Load class - some of this may be GUI
	byte _saveLoadFlag, _saveLoadSlot;
	uint32 _lastSaveTime;
	bool _saveTemporaryState;
	bool _pauseSoundsDuringSave;
	bool _loadFromLauncher;
	bool _videoModeChanged;
	Common::String _saveLoadFileName;
	Common::String _saveLoadDescription;

	bool saveState(Common::SeekableWriteStream *out, bool writeHeader = true);
	bool saveState(int slot, bool compat, Common::String &fileName);
	bool loadState(int slot, bool compat);
	bool loadState(int slot, bool compat, Common::String &fileName);
	void saveLoadWithSerializer(Common::Serializer &s) override;
	void saveResource(Common::Serializer &ser, ResType type, ResId idx);
	void loadResource(Common::Serializer &ser, ResType type, ResId idx);
	void loadResourceOLD(Common::Serializer &ser, ResType type, ResId idx);	// "Obsolete"

	virtual int checkSoundEngineSaveDataSize(Serializer &s);

	void copyHeapSaveGameToFile(int slot, const char *saveName);
	bool changeSavegameName(int slot, char *newName);
	virtual Common::SeekableReadStream *openSaveFileForReading(int slot, bool compat, Common::String &fileName);
	virtual Common::SeekableWriteStream *openSaveFileForWriting(int slot, bool compat, Common::String &fileName);

	Common::String makeSavegameName(int slot, bool temporary) const {
		return makeSavegameName(_targetName, slot, temporary);
	}

	int getKeyState(int key);
	int getActionState(ScummAction action);

public:
	static Common::String makeSavegameName(const Common::String &target, int slot, bool temporary);

	bool getSavegameName(int slot, Common::String &desc);
	void listSavegames(bool *marks, int num);

	void requestSave(int slot, const Common::String &name);
	void requestLoad(int slot);

	Common::String getTargetName() const { return _targetName; }
	bool canPauseSoundsDuringSave() const { return _pauseSoundsDuringSave; }

// thumbnail + info stuff
public:
	static bool querySaveMetaInfos(const char *target, int slot, int heversion, Common::String &desc, Graphics::Surface *&thumbnail, SaveStateMetaInfos *&timeInfos);

protected:
	void saveInfos(Common::WriteStream *file);
	static bool loadInfos(Common::SeekableReadStream *file, SaveStateMetaInfos *stuff);

	/* Script VM - should be in Script class */
	uint32 _localScriptOffsets[1024];
	Common::HashMap<int, byte *> _scriptOverrides;
	const byte *_scriptPointer;
	const byte *_scriptOrgPointer;
	const byte * const *_lastCodePtr;
	byte _opcode;
	bool _debug;
	byte _currentScript; // Let debug() work on init stage
	int _scummStackPos;
	int _vmStack[256];

	char _engineVersionString[50];
	char _dataFileVersionString[50];

	OpcodeEntry _opcodes[256];

	/**
	 * Small helper to avoid checking `_currentScript != 0xFF` before every
	 * `vm.slot[_currentScript].number` use that would require so.
	 */
	bool currentScriptSlotIs(uint16 script) const {
		return _currentScript != 0xFF && vm.slot[_currentScript].number == script;
	}

	virtual void setupOpcodes() = 0;
	void executeOpcode(byte i);
	const char *getOpcodeDesc(byte i);

	void initializeLocals(int slot, int *vars);
	int	getScriptSlot();

	void startScene(int room, Actor *a, int b);
	bool startManiac();

public:
	void runScript(int script, bool freezeResistant, bool recursive, int *lvarptr, int cycle = 0);
	void stopScript(int script);
	void nukeArrays(byte scriptSlot);

protected:
	void runObjectScript(int script, int entry, bool freezeResistant, bool recursive, int *vars, int slot = -1, int cycle = 0);
	void runScriptNested(int script);
	void executeScript();
	void updateScriptPtr();
	virtual void runInventoryScript(int i);
	virtual void runInventoryScriptEx(int i);
	virtual void checkAndRunSentenceScript();
	bool monkey1HermanNoteWorkaround(const SentenceTab &st);
	void runExitScript();
	void runEntryScript();
	void runQuitScript();
	void runAllScripts();
	void freezeScripts(int scr);
	void unfreezeScripts();

	bool isScriptInUse(int script) const;
	bool isRoomScriptRunning(int script) const;
	bool isScriptRunning(int script) const;

	void killAllScriptsExceptCurrent();
	void killScriptsAndResources();
	void decreaseScriptDelay(int amount);

	void stopObjectCode();
	void stopObjectScript(int script);

	void getScriptBaseAddress();
	void resetScriptPointer();
	int getVerbEntrypoint(int obj, int entry);

	void refreshScriptPointer();
	byte fetchScriptByte();
	virtual uint fetchScriptWord();
	virtual int fetchScriptWordSigned();
	uint fetchScriptDWord();
	int fetchScriptDWordSigned();
	void ignoreScriptWord() { fetchScriptWord(); }
	void ignoreScriptByte() { fetchScriptByte(); }
	void push(int a);
	int pop();
	virtual int readVar(uint var);
	virtual void writeVar(uint var, int value);
	// SCUMM 1/2
	virtual void resetSentence() {}

protected:
	virtual void beginCutscene(int *args);
	virtual void endCutscene();
	void abortCutscene();
	void beginOverride();
	void endOverride();

	void copyScriptString(byte *dst);
	int resStrLen(const byte *src);
	void doSentence(int c, int b, int a);

	/* Should be in Resource class */
	BaseScummFile *_fileHandle;
	uint32 _fileOffset;
public:
	/** The name of the (macintosh/rescumm style) container file, if any. */
	Common::Path _containerFile;
	Common::Path _macCursorFile;

	bool openFile(BaseScummFile &file, const Common::Path &filename, bool resourceFile = false);
	ScummFile *instantiateScummFile(bool indexPAKFiles = true);

protected:
	int _resourceHeaderSize;
	byte _resourceMapper[128];
	const byte *_resourceLastSearchBuf; // FIXME: need to put it to savefile?
	uint32 _resourceLastSearchSize;    // FIXME: need to put it to savefile?

	virtual void allocateArrays();
	void openRoom(int room);
	void closeRoom();
	void deleteRoomOffsets();
	virtual void readRoomsOffsets();
	void askForDisk(const Common::Path &filename, int disknum);
	byte getEncByte(int room);
	bool openResourceFile(const Common::Path &filename, byte encByte);

	void loadPtrToResource(ResType type, ResId idx, const byte *ptr);
	virtual int readResTypeList(ResType type);
//	void allocResTypeData(ResType type, uint32 tag, int num, int mode);
//	byte *createResource(int type, int index, uint32 size);
	int loadResource(ResType type, ResId idx);
//	void nukeResource(ResType type, ResId idx);
	int getResourceRoomNr(ResType type, ResId idx);
	virtual uint32 getResourceRoomOffset(ResType type, ResId idx);

	void scriptOverride(ResId room, int script);

public:
	int getResourceSize(ResType type, ResId idx);
	byte *getResourceAddress(ResType type, ResId idx);
	virtual byte *getStringAddress(ResId idx);
	byte *getStringAddressVar(int i);
	void ensureResourceLoaded(ResType type, ResId idx);

protected:
	Common::Mutex _resourceAccessMutex; // Used in getResourceSize(), getResourceAddress() and findResource()
										// to avoid race conditions between the audio thread of Digital iMUSE
										// and the main SCUMM thread

	int readSoundResource(ResId idx);
	int readSoundResourceSmallHeader(ResId idx);
	bool isResourceInUse(ResType type, ResId idx) const;

	virtual void setupRoomSubBlocks();
	virtual void resetRoomSubBlocks();

	virtual void clearRoomObjects();
	virtual void resetRoomObjects();
	virtual void resetRoomObject(ObjectData *od, const byte *room, const byte *searchptr = NULL);

	virtual void readArrayFromIndexFile();
	virtual void readMAXS(int blockSize) = 0;
	virtual void readGlobalObjects();
	virtual void readIndexFile();
	virtual void readIndexBlock(uint32 block, uint32 itemsize);
	virtual void loadCharset(int i);
	void nukeCharset(int i);

	int _lastLoadedRoom;
public:
	const byte *findResourceData(uint32 tag, const byte *ptr);
	const byte *findResource(uint32 tag, const byte *ptr);
	void applyWorkaroundIfNeeded(ResType type, int idx);
	bool verifyMI2MacBootScript();
	bool verifyMI2MacBootScript(byte *buf, int size);
	bool tryPatchMI1CannibalScript(byte *buf, int size);

	int getResourceDataSize(const byte *ptr) const;
	void dumpResource(const char *tag, int index, const byte *ptr, int length = -1);

public:
	/* Should be in Object class */
	byte OF_OWNER_ROOM;
	int getInventorySlot();
	int findInventory(int owner, int index);
	int getInventoryCount(int owner);

protected:
	byte *_objectOwnerTable, *_objectRoomTable, *_objectStateTable;
	int _numObjectsInRoom;

public:
	uint32 *_classData;

protected:
	void markObjectRectAsDirty(int obj);
	virtual void loadFlObject(uint object, uint room);
	void nukeFlObjects(int min, int max);
	int findFlObjectSlot();
	int findLocalObjectSlot();
	void addObjectToInventory(uint obj, uint room);
	void updateObjectStates();
public:
	bool getClass(int obj, int cls) const;		// Used in actor.cpp, hence public
protected:
	void putClass(int obj, int cls, bool set);
	int getState(int obj);
	void putState(int obj, int state);
	void setObjectState(int obj, int state, int x, int y);
	int getOwner(int obj) const;
	void putOwner(int obj, int owner);
	void setOwnerOf(int obj, int owner);
	void clearOwnerOf(int obj);
	int getObjectRoom(int obj) const;
	virtual bool objIsActor(int obj);
	virtual int objToActor(int obj);
	virtual int actorToObj(int actor);
	int getObjX(int obj);
	int getObjY(int obj);
	void getObjectWidth(int object, int &width) { int x, y, dir; getObjectXYPos(object, x, y, dir, width); }
	void getObjectXYPos(int object, int &x, int &y) { int dir, width; getObjectXYPos(object, x, y, dir, width); }
	void getObjectXYPos(int object, int &x, int &y, int &dir) { int width; getObjectXYPos(object, x, y, dir, width); }
	void getObjectXYPos(int object, int &x, int &y, int &dir, int &width);
	int getObjOldDir(int obj);
	int getObjNewDir(int obj);
	int getObjectIndex(int object) const;
	int getObjectImageCount(int object);
	int whereIsObject(int object) const;
	int findObject(int x, int y);
	void findObjectInRoom(FindObjectInRoom *fo, byte findWhat, uint object, uint room);
public:
	int getObjectOrActorWidth(int object, int &width); // Used in v4 and below
	int getObjectOrActorXY(int object, int &x, int &y);	// Used in actor.cpp, hence public
	int getDist(int x, int y, int x2, int y2);	// Also used in actor.cpp
protected:

	int getObjActToObjActDist(int a, int b); // Not sure how to handle
	const byte *getObjOrActorName(int obj);		 // these three..
	void setObjectName(int obj);

	void addObjectToDrawQue(int object);
	void removeObjectFromDrawQue(int object);
	void clearDrawObjectQueue();
	void processDrawQue();

	virtual void clearDrawQueues();

	uint32 getOBCDOffs(int object) const;
	byte *getOBCDFromObject(int obj, bool v0CheckInventory = true);
	const byte *getOBIMFromObjectData(const ObjectData &od);
	const byte *getObjectImage(const byte *ptr, int state);
	virtual int getObjectIdFromOBIM(const byte *obim);

protected:
	/* Should be in Verb class */
	uint16 _verbMouseOver;
	int8 _userPut;
	uint16 _userState;

	virtual void handleMouseOver(bool updateInventory);
	virtual void redrawVerbs();
	virtual void checkExecVerbs();

	void verbMouseOver(int verb);
	int findVerbAtPos(int x, int y) const;
	virtual void drawVerb(int verb, int mode, Common::TextToSpeechManager::Action ttsAction = Common::TextToSpeechManager::INTERRUPT);
	virtual void runInputScript(int clickArea, int val, int mode);
	void restoreVerbBG(int verb);
	void drawVerbBitmap(int verb, int x, int y);
	int getVerbSlot(int id, int mode) const;
	void killVerb(int slot);
	void setVerbObject(uint room, uint object, uint verb);

public:
	bool isValidActor(int id) const;

	/* Should be in Actor class */
	Actor *derefActor(int id, const char *errmsg = 0) const;
	Actor *derefActorSafe(int id, const char *errmsg) const;

protected:
	void walkActors();
	void playActorSounds();
	void redrawAllActors();
	virtual void setActorRedrawFlags();
	void putActors();
	void showActors();
	void resetV1ActorTalkColor();
	virtual void resetActorBgs();
	virtual void processActors();
	void processUpperActors();
	virtual int getActorFromPos(int x, int y);

public:
	/* Actor talking stuff */
	byte _actorToPrintStrFor, _V1TalkingActor;
	int _sentenceNum;
	SentenceTab _sentence[NUM_SENTENCE];
	StringTab _string[6];
	byte _haveMsg;
	int16 _talkDelay;
	int _NES_lastTalkingActor;
	int _NES_talkColor;

	virtual void actorTalk(const byte *msg);
	void stopTalk();
	int getTalkingActor();		// Wrapper around VAR_TALK_ACTOR for V1 Maniac
	void setTalkingActor(int variable);

	// Generic costume code
	bool isCostumeInUse(int i) const;

protected:
	/* Should be in Graphics class? */
	uint16 _screenB, _screenH;
public:
	int _roomHeight, _roomWidth;
	int _screenHeight, _screenWidth;
	VirtScreen _virtscr[4];		// Virtual screen areas
	CameraData camera;			// 'Camera' - viewport
	bool _cameraIsFrozen;

	int _screenStartStrip, _screenEndStrip;
	int _screenTop;
	bool _forceBannerVirtScreen;

	// For Mac versions of 320x200 games:
	// these versions rendered at 640x480 without any aspect ratio correction;
	// in order to correctly display the games as they should be, we perform some
	// offset corrections within the various rendering pipelines.
	//
	// The only reason I've made _useMacScreenCorrectHeight toggleable is because
	// maybe someday the screen correction can be activated or deactivated from the
	// ScummVM GUI; but currently I'm not taking that responsibility, after all the
	// work done on ensuring that old savegames translate correctly to the new setting... :-P
	bool _useMacScreenCorrectHeight;
	int _macScreenDrawOffset;

	Common::RenderMode _renderMode;
	uint8 _bytesPerPixel;
	Graphics::PixelFormat _outputPixelFormat;

protected:
	ColorCycle _colorCycle[16];	// Palette cycles
	uint8 _colorUsedByCycle[256];
	Graphics::PaletteLookup _pl; // Used by the internal GUI

	uint32 _ENCD_offs, _EXCD_offs;
	uint32 _CLUT_offs, _EPAL_offs;
	uint32 _IM00_offs, _PALS_offs;

	//ender: fullscreen
	bool _fullRedraw, _bgNeedsRedraw;
	bool _screenEffectFlag, _completeScreenRedraw;
	bool _disableFadeInEffect;

	struct {
		int hotspotX, hotspotY, width, height;
		byte animate, animateIndex;
		int8 state;
	} _cursor;

	// HACK Double the array size to handle 16-bit images.
	// this should be dynamically allocated based on game depth instead.
	byte _grabbedCursor[16384];
	byte _macGrabbedCursor[16384 * 4]; // Double resolution cursor
	byte _currentCursor;

	byte _newEffect, _switchRoomEffect2, _switchRoomEffect;
	bool _doEffect;

	bool _snapScroll;

	virtual void setBuiltinCursor(int index) {}
public:
	bool isLightOn() const;

	virtual int getCurrentLights() const;

protected:
	void initScreens(int b, int h);
	void initVirtScreen(VirtScreenNumber slot, int top, int width, int height, bool twobufs, bool scrollable);
	void initBGBuffers(int height);
	void initCycl(const byte *ptr);	// Color cycle

	void decodeNESBaseTiles();
	void playNESTitleScreens();

	void drawObject(int obj, int scrollType);
	void drawRoomObjects(int arg);
	void drawRoomObject(int i, int arg);
	void drawBox(int x, int y, int x2, int y2, int color);
	void drawLine(int x1, int y1, int x2, int y2, int color);
	void drawPixel(VirtScreen *vs, int x, int y, int16 color, bool useBackbuffer = false);

	void moveScreen(int dx, int dy, int height);

	void restoreBackground(Common::Rect rect, byte backcolor = 0);
	void redrawBGStrip(int start, int num);
	virtual void redrawBGAreas();

	void cameraMoved();
	void setCameraAtEx(int at);
	virtual void setCameraAt(int pos_x, int pos_y);
	virtual void setCameraFollows(Actor *a, bool setCamera = false);
	virtual void moveCamera();
	virtual void panCameraTo(int x, int y);
	void clampCameraPos(Common::Point *pt);
	void actorFollowCamera(int act);

	const byte *getPalettePtr(int palindex, int room);

	void setPaletteFromTable(const byte *ptr, int numcolor, int firstIndex = 0);
	void resetPalette(bool isBootUp = false);

	void setCurrentPalette(int pal);
	void setRoomPalette(int pal, int room);
	void setPCEPaletteFromPtr(const byte *ptr);
	void setAmigaPaletteFromPtr(const byte *ptr);
	virtual void setPaletteFromPtr(const byte *ptr, int numcolor = -1);
	void setV1ColorTable(int renderMode);

	virtual void setPalColor(int index, int r, int g, int b);
	void setDirtyColors(int min, int max);
	const byte *findPalInPals(const byte *pal, int index);
	void swapPalColors(int a, int b);
	virtual void copyPalColor(int dst, int src);
	void cyclePalette();
	void stopCycle(int i);
	virtual void palManipulateInit(int resID, int start, int end, int time);
	void palManipulate();
	uint32 findClosestPaletteColor(byte *palette, int paletteLength, byte r, byte g, byte b);
	void applyGrayscaleToPaletteRange(int min, int max); // For Sam&Max original noir mode
	bool haveToApplyMonkey1PaletteFix();

public:
	uint8 *getHEPaletteSlot(uint16 palSlot);
	uint16 get16BitColor(uint8 r, uint8 g, uint8 b);
	uint32 getPaletteColorFromRGB(byte *palette, byte r, byte g, byte b);
	uint32 getPackedRGBColorFromPalette(byte *palette, uint32 color);
	void fetchBlackAndWhite(uint32 &black, uint32 &white, byte *palette, int paletteEntries);
	int remapPaletteColor(int r, int g, int b, int threshold);		// Used by Actor::remapActorPalette
	void readPCEPalette(const byte **ptr, byte **dest, int numEntries);
	void colorPCEToRGB(uint16 color, byte *r, byte *g, byte *b);
	void setPCETextPalette(uint8 color);
protected:
	void moveMemInPalRes(int start, int end, byte direction);
	void setShadowPalette(int slot, int redScale, int greenScale, int blueScale, int startColor, int endColor);
	void setShadowPalette(int redScale, int greenScale, int blueScale, int startColor, int endColor, int start, int end);
	virtual void darkenPalette(int redScale, int greenScale, int blueScale, int startColor, int endColor);

public:
	void markRectAsDirty(VirtScreenNumber virt, int left, int right, int top, int bottom, int dirtybit = 0);
	void markRectAsDirty(VirtScreenNumber virt, const Common::Rect& rect, int dirtybit = 0) {
		markRectAsDirty(virt, rect.left, rect.right, rect.top, rect.bottom, dirtybit);
	}
protected:
	// Screen rendering
	byte *_compositeBuf;
	byte *_hercCGAScaleBuf;
	bool _enableEGADithering;
	bool _supportsEGADithering;
	bool _enableSegaShadowMode;

	virtual void drawDirtyScreenParts();
	void updateDirtyScreen(VirtScreenNumber slot);
	void drawStripToScreen(VirtScreen *vs, int x, int width, int top, int bottom);

	void mac_markScreenAsDirty(int x, int y, int w, int h);
	void mac_drawStripToScreen(VirtScreen *vs, int top, int x, int y, int width, int height);
	void mac_drawIndy3TextBox();
	void mac_undrawIndy3TextBox();
	void mac_undrawIndy3CreditsText();
	void mac_drawBufferToScreen(const byte *buffer, int pitch, int x, int y, int width, int height, bool epxRectangleExpansion = true);
	void mac_updateCompositeBuffer(const byte *buffer, int pitch, int x, int y, int width, int height);
	void mac_blitDoubleResImage(const byte *buffer, int pitch, int x, int y, int width, int height);
	void mac_applyDoubleResToBuffer(const byte *inputBuffer, byte *outputBuffer, int width, int height, int inputPitch, int outputPitch);
	void mac_blitEPXImage(const byte *buffer, int pitch, int x, int y, int width, int height, bool epxRectangleExpansion = true);
	void mac_applyEPXToBuffer(const byte *inputBuffer, byte *outputBuffer, int width, int height, int inputPitch, int outputPitch, int xOffset, int yOffset, int bufferWidth, int bufferHeight);
	void mac_scaleCursor(byte *&outCursor, int &outHotspotX, int &outHotspotY, int &outWidth, int &outHeight);
	void mac_toggleSmoothing();

	Common::KeyState mac_showOldStyleBannerAndPause(const char *msg, int32 waitTime);

	const byte *postProcessDOSGraphics(VirtScreen *vs, int &pitch, int &x, int &y, int &width, int &height) const;
	const byte *ditherVGAtoEGA(int &pitch, int &x, int &y, int &width, int &height) const;

public:
	VirtScreen *findVirtScreen(int y);
	byte *getMaskBuffer(int x, int y, int z);

protected:
	void fadeIn(int effect);
	void fadeOut(int effect);

	void dissolveEffectSelector();
	void transitionEffect(int a);
	void dissolveEffect(int width, int height);
	void scrollEffect(int dir);

	void updateScreenShakeEffect();

public:
	double getTimerFrequency();
	double getAmigaMusicTimerFrequency(); // For setting up Players v2 and v3

protected:
	bool _shakeEnabled;
	bool _shakeTempSavedState; // For saving and restoring before and after GUI calls
	uint _shakeFrame;
	uint32 _shakeNextTick;
	uint32 _shakeTickCounter;
	double _shakeTimerRate;
	double _timerFrequency;

	void setShake(int mode);

	int _drawObjectQueNr;
	byte _drawObjectQue[200];

	/* For each of the 410 screen strips, gfxUsageBits contains a
	 * bitmask. The lower 80 bits each correspond to one actor and
	 * signify if any part of that actor is currently contained in
	 * that strip.
	 *
	 * If the leftmost bit is set, the strip (background) is dirty
	 * needs to be redrawn.
	 *
	 * The second leftmost bit is set by restoreBlastObjectsRect() and
	 * restoreBackground(), but I'm not yet sure why.
	 */
	uint32 gfxUsageBits[410 * 3];

	void upgradeGfxUsageBits();
	void setGfxUsageBit(int strip, int bit);
	void clearGfxUsageBit(int strip, int bit);

	// speed optimization: inline due to frequent calling
	bool testGfxUsageBit(int strip, int bit) {
		assert(strip >= 0 && strip < ARRAYSIZE(gfxUsageBits) / 3);
		assert(1 <= bit && bit <= 96);
		bit--;
		return (gfxUsageBits[3 * strip + bit / 32] & (1 << (bit % 32))) != 0;
	}

	bool testGfxAnyUsageBits(int strip);
	bool testGfxObjectUsageBits(int strip); // Used for HE actors overlap calculations
	bool testGfxOtherUsageBits(int strip, int bit);

public:
	byte _roomPalette[256];
	byte *_shadowPalette;
	bool _skipDrawObject;
	int _voiceMode;
	int _soundEnabled;

	// HE specific
	byte _HEV7ActorPalette[256];
	uint8 *_hePalettes;
	uint16 _hePaletteSlot;
	uint16 *_16BitPalette;

	// Indy4 Amiga specific
	byte *_verbPalette;

	ScummEngine_v0_Delays _V0Delay;

protected:
	int _shadowPaletteSize;
	byte _currentPalette[3 * 256];
	byte _darkenPalette[3 * 256];
	int _paletteChangedCounter;

	int _palDirtyMin, _palDirtyMax;

	byte _palManipStart, _palManipEnd;
	uint16 _palManipCounter;
	byte *_palManipPalette;
	byte *_palManipIntermediatePal;

	bool _haveActorSpeechMsg;
	bool _useTalkAnims;
	uint16 _defaultTextSpeed;
	int _saveSound;
	bool _native_mt32;
	bool _copyProtection;
	bool _shadowPalRemap;

	// Indy4 Amiga specific
	uint16 _amigaFirstUsedColor;
	byte _amigaPalette[3 * 64];
	void amigaPaletteFindFirstUsedColor();
	void mapRoomPalette(int idx);
	int remapRoomPaletteColor(int r, int g, int b);
	void mapVerbPalette(int idx);
	int remapVerbPaletteColor(int r, int g, int b);

	// EGA dithering mode color tables for certain VGA games like MI2, LOOM Talkie...
	byte *_egaColorMap[2];

public:
	uint16 _extraBoxFlags[65];

	byte getNumBoxes();
	byte *getBoxMatrixBaseAddr();
	byte *getBoxConnectionBase(int box);

	int getNextBox(byte from, byte to);

	void setBoxFlags(int box, int val);
	void setBoxScale(int box, int b);

	bool checkXYInBoxBounds(int box, int x, int y);

	BoxCoords getBoxCoordinates(int boxnum);

	byte getMaskFromBox(int box);
	Box *getBoxBaseAddr(int box);
	byte getBoxFlags(int box);
	int getBoxScale(int box);

	int getScale(int box, int x, int y);
	int getScaleFromSlot(int slot, int x, int y);

protected:
	// Scaling slots/items
	struct ScaleSlot {
		int x1, y1, scale1;
		int x2, y2, scale2;
	};
	friend void syncWithSerializer(Common::Serializer &, ScaleSlot &);
	ScaleSlot _scaleSlots[20];
	void setScaleSlot(int slot, int x1, int y1, int scale1, int x2, int y2, int scale2);
	void setBoxScaleSlot(int box, int slot);
	void convertScaleTableToScaleSlot(int slot);

	void calcItineraryMatrix(byte *itineraryMatrix, int num);
	void createBoxMatrix();
	virtual bool areBoxesNeighbors(int i, int j);

	/* String class */
public:
	CharsetRenderer *_charset;
	byte _charsetColorMap[16];

	/**
	 * All text is normally rendered into this overlay surface. Then later
	 * drawStripToScreen() composits it over the game graphics.
	 */
	Graphics::Surface _textSurface;
	int _textSurfaceMultiplier;

	bool _isModernMacVersion;
	bool _useGammaCorrection;

	Graphics::Surface *_macScreen;
	MacGui *_macGui;
	bool _useMacGraphicsSmoothing;
	byte _completeScreenBuffer[320 * 200];

protected:
	byte _charsetColor;
	byte _charsetData[23][16];

	int _charsetBufPos;
	byte _charsetBuffer[512];

	bool _keepText;
	byte _msgCount;

	int _nextLeft, _nextTop;

	Localizer *_localizer;

#ifdef USE_TTS
	bool _voiceNextString = false;
	bool _checkPreviousSaid = false;
	bool _voicePassHelpButtons = false;
	int _previousVerb = -1;
	int _previousControl = -1;
	Common::String _previousSaid;
	Common::String _passHelpButtons[6];
#endif

	void restoreCharsetBg();
	void clearCharsetMask();
	void clearTextSurface();

	virtual void initCharset(int charset);

	virtual void printString(int m, const byte *msg);

	virtual bool handleNextCharsetCode(Actor *a, int *c);
	virtual void drawSentence() {}
	virtual void displayDialog();
	int countNumberOfWaits(); // For SE speech support, from disasm
	bool newLine();
	void drawString(int a, const byte *msg, Common::TextToSpeechManager::Action ttsAction = Common::TextToSpeechManager::QUEUE);
	virtual void fakeBidiString(byte *ltext, bool ignoreVerb, int ltextSize) const;
	void wrapSegaCDText();
	void debugMessage(const byte *msg);
	virtual void showMessageDialog(const byte *msg);

#ifdef USE_TTS
	void sayText(const Common::String &text, Common::TextToSpeechManager::Action action = Common::TextToSpeechManager::QUEUE) const;
	void stopTextToSpeech() const;
	void sayButtonText();
#endif

	virtual int convertMessageToString(const byte *msg, byte *dst, int dstSize);
	int convertIntMessage(byte *dst, int dstSize, int var);
	int convertVerbMessage(byte *dst, int dstSize, int var);
	int convertNameMessage(byte *dst, int dstSize, int var);
	int convertStringMessage(byte *dst, int dstSize, int var);

public:
	Common::Language _language;	// Accessed by a hack in NutRenderer::loadFont

	// Used by class ScummDialog:
	virtual void translateText(const byte *text, byte *trans_buff, int transBufferSize);
	// Old Hebrew games require reversing the dialog text.
	bool reverseIfNeeded(const byte *text, byte *reverseBuf, int reverseBufSize) const;
	// Returns codepage that matches the game for languages that require it.
	Common::CodePage getDialogCodePage() const;

	// Somewhat hackish stuff for 2 byte support (Chinese/Japanese/Korean)
	bool _useCJKMode;
	bool _useMultiFont;
	int _numLoadedFont;
	int _2byteShadow;
	bool _force2ByteCharHeight;

	int _2byteHeight;
	int _2byteWidth;
	int _krStrPost;
	byte _newLineCharacter;
	byte *get2byteCharPtr(int idx);

	bool isScummvmKorTarget();
	bool hasLocalizer();

//protected:
	byte *_2byteFontPtr;
	byte *_2byteMultiFontPtr[20];
	int _2byteMultiHeight[20];
	int _2byteMultiWidth[20];
	int _2byteMultiShadow[20];

private:
	struct TranslatedLine {
		uint32 originalTextOffset;
		uint32 translatedTextOffset;
	};

	struct TranslationRange {
		uint32 left;
		uint32 right;

		TranslationRange(uint32 left_, uint32 right_) : left(left_), right(right_) {}
		TranslationRange() : left(0), right(0) {}
	};

	struct TranslationRoom {
		Common::HashMap<uint32, TranslationRange> scriptRanges;
	};

	bool _existLanguageFile;
	bool _isRTL;
	byte *_languageBuffer;
	int _numTranslatedLines;
	TranslatedLine *_translatedLines;
	uint16 *_languageLineIndex;
	Common::HashMap<byte, TranslationRoom> _roomIndex;

	const byte *searchTranslatedLine(const byte *text, const TranslationRange &range, bool useIndex);

	virtual void createTextRenderer(GlyphRenderer_v7 *gr) {}

public:

	/* Scumm Vars */
	byte VAR_KEYPRESS;
	byte VAR_SYNC;
	byte VAR_EGO;
	byte VAR_CAMERA_POS_X;
	byte VAR_HAVE_MSG;
	byte VAR_ROOM;
	byte VAR_OVERRIDE;
	byte VAR_MACHINE_SPEED;
	byte VAR_ME;
	byte VAR_NUM_ACTOR;
	byte VAR_CURRENT_LIGHTS;
	byte VAR_CURRENTDRIVE; // How about merging this with VAR_CURRENTDISK?
	byte VAR_CURRENTDISK;
	byte VAR_TMR_1;
	byte VAR_TMR_2;
	byte VAR_TMR_3;
	byte VAR_MUSIC_TIMER;
	byte VAR_ACTOR_RANGE_MIN;
	byte VAR_ACTOR_RANGE_MAX;
	byte VAR_CAMERA_MIN_X;
	byte VAR_CAMERA_MAX_X;
	byte VAR_TIMER_NEXT;
	byte VAR_VIRT_MOUSE_X;
	byte VAR_VIRT_MOUSE_Y;
	byte VAR_ROOM_RESOURCE;
	byte VAR_LAST_SOUND;
	byte VAR_CUTSCENEEXIT_KEY;
	byte VAR_OPTIONS_KEY;
	byte VAR_TALK_ACTOR;
	byte VAR_CAMERA_FAST_X;
	byte VAR_SCROLL_SCRIPT;
	byte VAR_ENTRY_SCRIPT;
	byte VAR_ENTRY_SCRIPT2;
	byte VAR_EXIT_SCRIPT;
	byte VAR_EXIT_SCRIPT2;
	byte VAR_VERB_SCRIPT;
	byte VAR_SENTENCE_SCRIPT;
	byte VAR_INVENTORY_SCRIPT;
	byte VAR_CUTSCENE_START_SCRIPT;
	byte VAR_CUTSCENE_END_SCRIPT;
	byte VAR_CHARINC;
	byte VAR_WALKTO_OBJ;
	byte VAR_DEBUGMODE;
	byte VAR_HEAPSPACE;
	byte VAR_RESTART_KEY;
	byte VAR_PAUSE_KEY;
	byte VAR_MOUSE_X;
	byte VAR_MOUSE_Y;
	byte VAR_TIMER;
	byte VAR_TIMER_TOTAL;
	byte VAR_SOUNDCARD;
	byte VAR_VIDEOMODE;
	byte VAR_MAINMENU_KEY;
	byte VAR_FIXEDDISK;
	byte VAR_CURSORSTATE;
	byte VAR_USERPUT;
	byte VAR_SOUNDRESULT;
	byte VAR_TALKSTOP_KEY;
	byte VAR_FADE_DELAY;
	byte VAR_NOSUBTITLES;

	// V5+
	byte VAR_SOUNDPARAM;
	byte VAR_SOUNDPARAM2;
	byte VAR_SOUNDPARAM3;
	byte VAR_INPUTMODE;
	byte VAR_MEMORY_PERFORMANCE;
	byte VAR_VIDEO_PERFORMANCE;
	byte VAR_ROOM_FLAG;
	byte VAR_GAME_LOADED;
	byte VAR_NEW_ROOM;

	// V4/V5
	byte VAR_V5_TALK_STRING_Y;

	// V6+
	byte VAR_ROOM_WIDTH;
	byte VAR_ROOM_HEIGHT;
	byte VAR_SUBTITLES;
	byte VAR_V6_EMSSPACE;

	// V7/V8 specific variables
	byte VAR_CAMERA_POS_Y;
	byte VAR_CAMERA_MIN_Y;
	byte VAR_CAMERA_MAX_Y;
	byte VAR_CAMERA_THRESHOLD_X;
	byte VAR_CAMERA_THRESHOLD_Y;
	byte VAR_CAMERA_SPEED_X;
	byte VAR_CAMERA_SPEED_Y;
	byte VAR_CAMERA_ACCEL_X;
	byte VAR_CAMERA_ACCEL_Y;
	byte VAR_CAMERA_DEST_X;
	byte VAR_CAMERA_DEST_Y;
	byte VAR_CAMERA_FOLLOWED_ACTOR;

	// V7/V8 specific variables
	byte VAR_VERSION_KEY;
	byte VAR_DEFAULT_TALK_DELAY;
	byte VAR_CUSTOMSCALETABLE;
	byte VAR_BLAST_ABOVE_TEXT;
	byte VAR_VOICE_MODE;
	byte VAR_MUSIC_BUNDLE_LOADED;
	byte VAR_VOICE_BUNDLE_LOADED;

	byte VAR_LEFTBTN_DOWN;	// V7/V8
	byte VAR_RIGHTBTN_DOWN;	// V7/V8
	byte VAR_LEFTBTN_HOLD;	// V6/V72HE/V7/V8
	byte VAR_RIGHTBTN_HOLD;	// V6/V72HE/V7/V8
	byte VAR_PRE_SAVELOAD_SCRIPT;	// V6/V7 (not HE)
	byte VAR_POST_SAVELOAD_SCRIPT;	// V6/V7 (not HE)
	byte VAR_SAVELOAD_PAGE;		// V8
	byte VAR_OBJECT_LABEL_FLAG;	// V8

	// V6/V7 specific variables (FT & Sam & Max specific)
	byte VAR_CHARSET_MASK;

	// V6 specific variables
	byte VAR_V6_SOUNDMODE;

	// V1/V2 specific variables
	byte VAR_CHARCOUNT;
	byte VAR_VERB_ALLOWED;
	byte VAR_ACTIVE_VERB;
	byte VAR_ACTIVE_OBJECT1;
	byte VAR_ACTIVE_OBJECT2;

	// HE specific variables
	byte VAR_ALWAYS_REDRAW_ACTORS;		// Used in setActorRedrawFlags()
	byte VAR_SKIP_RESET_TALK_ACTOR;	// Used in setActorCostume()

	byte VAR_SOUND_CHANNEL;				// Used in o_startSound()
	byte VAR_TALK_CHANNEL;				// Used in playVoice()
	byte VAR_SOUND_TOKEN_OFFSET;			// Used in handleSoundFrame()
	byte VAR_START_DYN_SOUND_CHANNELS;	// Used in getNextDynamicChannel()
	byte VAR_SOUND_CALLBACK_SCRIPT;

	byte VAR_EARLY_TALKIE_CALLBACK;
	byte VAR_EARLY_CHAN_0_CALLBACK;
	byte VAR_EARLY_CHAN_1_CALLBACK;
	byte VAR_EARLY_CHAN_2_CALLBACK;
	byte VAR_EARLY_CHAN_3_CALLBACK;

	byte VAR_MAIN_SCRIPT; // Used in scummLoop()

	byte VAR_DEFAULT_SCRIPT_PRIORITY; // Used in runScript()/runObjectScript()
	byte VAR_LAST_SCRIPT_PRIORITY;    // Used in runAllScripts()

	byte VAR_QUIT_SCRIPT; // Used in confirmExitDialog()
	byte VAR_ERROR_FLAG; // HE70-90
	byte VAR_OPERATION_FAILURE; // HE99+

	byte VAR_COLOR_BLACK;

	// Exists both in V7 and in V72HE:
	byte VAR_NUM_GLOBAL_OBJS;

	byte VAR_LAST_FRAME_BURN_TIME;  // HE90+
	byte VAR_LAST_FRAME_SCUMM_TIME; // HE90+

	byte VAR_WINDEX_RUNNING;

#ifdef USE_RGB_COLOR
	// FM-Towns / PC-Engine specific
	Graphics::FontSJIS *_cjkFont;
#endif

	// FM-Towns specific
#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
public:
	bool towns_isRectInStringBox(int x1, int y1, int x2, int y2);
	byte _townsPaletteFlags;
	byte _townsCharsetColorMap[16];

protected:
	void towns_drawStripToScreen(VirtScreen *vs, int dstX, int dstY, int srcX, int srcY, int w, int h);
	void towns_fillTopLayerRect(int x1, int y1, int x2, int y2, int col);
	void towns_swapVirtScreenArea(VirtScreen *vs, int x, int y, int w, int h);
	void towns_clearStrip(int strip);
#ifdef USE_RGB_COLOR
	void towns_setPaletteFromPtr(const byte *ptr, int numcolor = -1);
	void towns_setTextPaletteFromPtr(const byte *ptr);
#endif
	void towns_setupPalCycleField(int x1, int y1, int x2, int y2);
	void towns_processPalCycleField();
	void towns_resetPalCycleFields();
	void towns_restoreCharsetBg();
	void towns_scriptScrollEffect(int dir);

	void requestScroll(int dir);
	void scrollLeft() {	requestScroll(-1); }
	void scrollRight() { requestScroll(1); }
	void towns_waitForScroll(int waitForDirection, int threshold = 0);
	void towns_updateGfx();

	Common::Rect _cyclRects[10];
	int _numCyclRects;
	int _scrollRequest;
	int _scrollDeltaAdjust;
	bool _scrollNeedDeltaAdjust;
	int _refreshDuration[20];
	int _refreshArrayPos;
	bool _refreshNeedCatchUp;
	bool _enableSmoothScrolling;
	bool _forceFMTownsHiResMode;
	uint32 _scrollTimer;
	uint32 _scrollDestOffset;
	uint16 _scrollFeedStrips[3];

	Common::Rect _curStringRect;

	byte _townsOverrideShadowColor;
	byte _textPalette[48];
	byte _townsClearLayerFlag;
	byte _townsActiveLayerFlags;
	static const uint8 _townsLayer2Mask[];

	TownsScreen *_townsScreen;
#else
	void scrollLeft() { redrawBGStrip(_gdi->_numStrips - 1, 1); }
	void scrollRight() { redrawBGStrip(0, 1); }
	void towns_updateGfx() {}
	void towns_waitForScroll(int waitForDirection, int threshold = 0) {}
	void towns_fillTopLayerRect(int x1, int y1, int x2, int y2, int col) {}
	void towns_swapVirtScreenArea(VirtScreen *vs, int x, int y, int w, int h) {}
#endif // DISABLE_TOWNS_DUAL_LAYER_MODE
};

} // End of namespace Scumm

#endif
