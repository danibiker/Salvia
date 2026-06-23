#ifndef _LOCALISE_H_
#define _LOCALISE_H_

extern bool bLanguageActive;
extern TCHAR szLanguage[MAX_PATH];
extern HMODULE hLanguage;

HMODULE FBALocaliseInstance();

void FBALocaliseExit();
int FBALocaliseInit(TCHAR* lanaugae);
int FBALocaliseLoad();

HBITMAP FBALoadBitmap(int id);
HMENU FBALoadMenu(int id);

int FBALoadString(UINT id, LPTSTR buffer, int maxsize);
TCHAR* FBALoadStringEx(UINT id, bool translate = true);

#define MAKEINTRESOURCE(x) x

#define    IDS_FIRSTRUN1				"This appears to be the first time you run %s v%.20s.\n\n"
#define    IDS_FIRSTRUN2				"Please take some time to match the audio/video settings to your system capabilities and personal preferences. If you continue to use and/or distribute this software, you must agree to be bound by the terms of license agreement, which is always accessible via the about dialog.\n\n"
#define    IDS_FIRSTRUN3A				"Note that this is alpha quality software. As such, it likely will exhibit problems, including audio/video options not behaving correctly, the emulation of some games misses some features, and may exhibit stability problems. CAVEAT LECTOR!"
#define    IDS_FIRSTRUN3B				"Note that this is beta quality software. As such, it likely may be missing some features, or exhibit some problems with your system."

#define	IDS_CHEAT_NAME				"Cheat name"
#define	IDS_CHEAT_STATUS			"Status"

#define	IDS_INPUT_INPUT				"Game input"
#define	IDS_INPUT_MAPPING			"Mapped to"
#define	IDS_INPUT_STATE				"State"
#define	IDS_INPUT_INP_PLAYER		"Player %i"
#define	IDS_INPUT_INP_KEYBOARD		"Keyboard"
#define	IDS_INPUT_INP_JOYSTICK		"Joystick %i"
#define	IDS_INPUT_INP_XARCADEL		"X-Arcade (left side)"
#define	IDS_INPUT_INP_XARCADER		"X-Arcade (right side)"
#define	IDS_INPUT_INP_HOTRODL		"Hot Rod (left side)"
#define	IDS_INPUT_INP_HOTRODR		"Hot Rod (right side)"
#define	IDS_INPUT_ANALOG_ABS		"Absolute"
#define	IDS_INPUT_ANALOG_AUTO		"Auto-center"
#define	IDS_INPUT_ANALOG_NORMAL		"Normal"
 
#define	IDS_DIP_GROUP				"DIPSwitch group"
#define	IDS_DIP_SETTING				"Setting"

#define	IDS_INPSET_MOVENAME			"Move control or press key for %.100hs"
#define	IDS_INPSET_MOVE				"Move control or press key"
#define	IDS_INPSET_WAITING			"Waiting for %.80s to be released..."

#define	IDS_MEMCARD_CREATE			"Create new memory card"
#define	IDS_MEMCARD_SELECT			"Select new memory card"

#define	IDS_PROGRESS_LOADING		"Loading and initialising %s..."
#define	IDS_PROGRESS_LOADING_ONLY	"Loading"

#define	IDS_REPLAY_RECORD			"Record Input to File"
#define	IDS_REPLAY_REPLAY			"Replay Input from File"

#define	IDS_ROMS_SELECT_DIR			"Select Directory:"

#define	IDS_STATE_LOAD				"Load State"
#define	IDS_STATE_SAVE				"Save State"
#define	IDS_STATE_ACTIVESLOT		"slot %d active"
#define	IDS_STATE_LOADED			"state loaded"
#define	IDS_STATE_LOAD_ERROR		"load error"
#define	IDS_STATE_SAVED				"state saved"
#define	IDS_STATE_SAVE_ERROR		"save error"

#define	IDS_DAT_GENERATE			"Generate %s dat"
#define	IDS_SCRN_NOGAME				"no game loaded"

#define	IDS_NETPLAY_START			"*** Starting netplay: "
#define	IDS_NETPLAY_START_YOU		"*** You are player %i. "
#define	IDS_NETPLAY_START_TOTAL		"There are %i total players."
#define	IDS_NETPLAY_DROP			"*** Player %1i <%.32hs> dropped from the game."
#define	IDS_NETPLAY_TOOMUCH			"too much text"

#define	IDS_SSHOT_SAVED				"PNG saved"
#define	IDS_SSHOT_ERROR				"PNG error %1d"

#define	IDS_SOUND_NOVOLUME			"No volume control"
#define	IDS_SOUND_VOLUMESET			"volume %03i %%"
#define	IDS_SOUND_MUTE				"Silence"
#define	IDS_SOUND_MUTE_OFF			"Sound Restored"

#define	IDS_DISK_REPLAY				"This input recording"
#define	IDS_DISK_THIS_REPLAY		"input recording."
#define	IDS_DISK_FILE_REPLAY		"%s Input Recording Files"
#define	IDS_DISK_STATE				"state file"
#define	IDS_DISK_THIS_STATE			"This state file"
#define	IDS_DISK_FILE_STATE			"%s State Files"
#define	IDS_DISK_FILE_PATCH			"%s Patch Files"
#define	IDS_DISK_SOUND				"sound recording"
#define	IDS_DISK_THIS_SOUND			"This sound recording"
#define	IDS_DISK_FILE_SOUND			"%s Sound Recording Files"
#define	IDS_DISK_FILE_CARD			"%s Memory Card Files"
#define	IDS_DISK_ALL_CARD			"All Memory Card Files"
#define	IDS_DISK_ALL_DAT			"All Dat Files"
#define	IDS_DISK_TEXT				"Text Files"

#define	IDS_DISK_SAVEREPORT			"Save system information report"
	
#define	IDS_GAMELANG_LOAD			"Select game list localisation template"
#define	IDS_GAMELANG_EXPORT			"Create game list localisation template"
	
#define	IDS_ROMINFO_DIALOGTITLE		"Rom Information"
#define	IDS_ROMINFO_ROMS			"Rom Info"
#define	IDS_ROMINFO_SAMPLES			"Sample Info"
#define	IDS_ROMINFO_NAME			"Name"
#define	IDS_ROMINFO_SIZE			"Size (bytes)"
#define	IDS_ROMINFO_CRC32			"CRC32"
#define	IDS_ROMINFO_TYPE			"Type"
#define	IDS_ROMINFO_FLAGS			"Flags"
#define	IDS_ROMINFO_ESSENTIAL		"%s, Essential"
#define	IDS_ROMINFO_OPTIONAL		"%s, Optional"
#define	IDS_ROMINFO_PROGRAM			"%s, Program"
#define	IDS_ROMINFO_GRAPHICS		"%s, Graphics"
#define	IDS_ROMINFO_SOUND			"%s, Sound"
#define	IDS_ROMINFO_BIOS			"%s, BIOS"
#define	IDS_ROMINFO_NODUMP			"%s, No Dump"
	
#define	IDS_SPATH_PREVIEW			"Previews"
#define	IDS_SPATH_TITLES			"Titles"
#define	IDS_SPATH_ICONS				"Icons"
#define	IDS_SPATH_CHEATS			"Cheats"
#define	IDS_SPATH_HISCORE			"High Scores"
#define	IDS_SPATH_SAMPLES			"Samples"
#define	IDS_SPATH_IPS				"IPS"
#define	IDS_SPATH_NGCD_ISOS			"Neo Geo CDZ Games"
#define	IDS_SPATH_NGCD_COVERS		"Neo Geo CDZ Covers"
 
#define	IDS_ROMPATH_1				"Path #1"
#define	IDS_ROMPATH_2				"Path #2"
#define	IDS_ROMPATH_3				"Path #3"
#define	IDS_ROMPATH_4				"Path #4"
#define	IDS_ROMPATH_5				"Path #5"
#define	IDS_ROMPATH_6				"Path #6"
#define	IDS_ROMPATH_7				"Path #7"
#define	IDS_ROMPATH_8				"Path #8"
#define	IDS_ROMPATH_9				"Path #9"
#define	IDS_ROMPATH_10				"Path #10"
#define	IDS_ROMPATH_11				"Path #11"
#define	IDS_ROMPATH_12				"Path #12"
#define	IDS_ROMPATH_13				"Path #13"
#define	IDS_ROMPATH_14				"Path #14"
#define	IDS_ROMPATH_15				"Path #15"
#define	IDS_ROMPATH_16				"Path #16"
#define	IDS_ROMPATH_17				"Path #17"
#define	IDS_ROMPATH_18				"Path #18"
#define	IDS_ROMPATH_19				"Path #19"
#define	IDS_ROMPATH_20				"Path #20"
	
#define	IDS_PRESET_SAVED			"The preset file was saved to %s."
	
#define	IDS_LOCAL_GL_CREATE			"Create gamelist localisation template"
#define	IDS_LOCAL_GL_SELECT			"Select gamelist localisation template"
#define	IDS_LOCAL_GL_FILTER			"FB Alpha gamelist localisation templates"
	
#define	IDS_CD_SELECT_IMAGE			"Select CD-ROM image..."
#define	IDS_CD_SELECT_IMAGE_TITLE	"Select CD-ROM image"
#define	IDS_CD_SELECT_FILTER		"CD images"
	
#define	IDS_LOCAL_CREATE			"Create localisation template"
#define	IDS_LOCAL_SELECT			"Select localisation template"
#define	IDS_LOCAL_FILTER			"FB Alpha localisation templates"
	
#define	IDS_SEL_FILTERS				"Filters"
#define	IDS_SEL_HARDWARE			"Hardware"
#define	IDS_SEL_CAVE				"Cave"
#define	IDS_SEL_CPS1				"CPS-1"
#define	IDS_SEL_CPS2				"CPS-2"
#define	IDS_SEL_CPS3				"CPS-3"
#define	IDS_SEL_NEOGEO				"Neo Geo"
#define	IDS_SEL_PGM					"PGM"
#define	IDS_SEL_PSIKYO				"Psikyo"
#define	IDS_SEL_SEGA				"Sega"
#define	IDS_SEL_TAITO				"Taito"
#define	IDS_SEL_TOAPLAN				"Toaplan"
#define	IDS_SEL_MISCPRE90S			"Misc (pre 90s)"
#define	IDS_SEL_MISCPOST90S			"Misc (post 90s)"
#define	IDS_SEL_MEGADRIVE			"Megadrive"
#define	IDS_SEL_PACMAN				"Pacman"
#define	IDS_SEL_KONAMI				"Konami"
#define	IDS_SEL_GALAXIAN			"Galaxian"
#define	IDS_SEL_IREM				"Irem"
#define	IDS_SEL_BOARDTYPE			"Board Type"
#define	IDS_SEL_BOOTLEG				"Bootleg"
#define	IDS_SEL_PROTOTYPE			"Prototype"
#define	IDS_SEL_HACK				"Hack"
#define	IDS_SEL_HOMEBREW			"Homebrew"
#define	IDS_SEL_DEMO				"Demo"
#define	IDS_SEL_GENUINE				"Genuine"
#define	IDS_SEL_SETSTATUS			"Showing %i of %i sets [%i Unavailable sets]"
#define	IDS_SEL_KANEKO				"Kaneko"
#define	IDS_SEL_SNES				"SNES"
#define	IDS_SEL_DATAEAST			"Data East"
#define	IDS_SEL_CAPCOM_MISC			"Capcom (Other)"
#define	IDS_SEL_SETA				"Seta"
#define	IDS_SEL_TECHNOS				"Technos"
#define	IDS_SEL_PCE					"PC-Engine"
	
#define	IDS_GENRE					"Genre"
#define	IDS_GENRE_HORSHOOT			"Shooter - Horizontal"
#define	IDS_GENRE_VERSHOOT			"Shooter - Vertical"
#define	IDS_GENRE_SCRFIGHT			"Fighter - Scrolling"
#define	IDS_GENRE_VSFIGHT			"Fighter - Versus"
#define	IDS_GENRE_BIOS				"BIOS"
#define	IDS_GENRE_BREAKOUT			"Breakout"
#define	IDS_GENRE_CASINO			"Casino"
#define	IDS_GENRE_BALLPADDLE		"Ball & Paddle"
#define	IDS_GENRE_MAZE				"Maze"
#define	IDS_GENRE_MINIGAMES			"Mini-Games"
#define	IDS_GENRE_PINBALL			"Pinball"
#define	IDS_GENRE_PLATFORM			"Platform"
#define	IDS_GENRE_PUZZLE			"Puzzle"
#define	IDS_GENRE_QUIZ				"Quiz"
#define	IDS_GENRE_SPORTSMISC		"Sports - Misc"
#define	IDS_GENRE_SPORTSFOOTBALL	"Sports - Football"
#define	IDS_GENRE_MISC				"Misc"
#define	IDS_GENRE_MAHJONG			"Mahjong"
#define	IDS_GENRE_RACING			"Racing"
#define	IDS_GENRE_SHOOT				"Shooter - Other"

#define	IDS_FAMILY					"Family"
#define	IDS_FAMILY_MSLUG			"Metal Slug"
#define	IDS_FAMILY_SF				"Street Fighter"
#define	IDS_FAMILY_KOF				"King of Fighters"
#define	IDS_FAMILY_DSTLK			"Darkstalkers"
#define	IDS_FAMILY_FATFURY			"Fatal Fury"
#define	IDS_FAMILY_SAMSHO			"Samurai Shodown"
#define	IDS_FAMILY_19XX				"19XX"
#define	IDS_FAMILY_SONICWI			"Aero Fighters"
#define	IDS_FAMILY_PWRINST			"Power Instinct"
#define	IDS_FAMILY_OTHER			"Other"
	
#define	IDS_IPSMANAGER_TITLE		"IPS Patch Manager"
	
#define	IDS_NGCD_DIAG_TITLE			"%s%sNeo Geo CDZ%sSelect Game"
#define	IDS_NGCD_TITLE				"Title"
#define	IDS_NGCD_NGCDID				"NGCD-ID"
	
#define	IDS_MENU_0 					"&Arcade"
#define IDS_MENU_1 					"&Other..."
#define IDS_MENU_2 					"&Other Gamma value..."
#define IDS_MENU_3 					"Adjust &CPU speed..."
#define IDS_MENU_4 					"Load state from active slot (no %d)\tF9"
#define IDS_MENU_5 					"Save state to active slot (no %d)\tF10"
#define	IDS_MENU_6					"no game selected"
	
#define	IDS_NUMDIAL_NUM_FRAMES		"Number of Frames"
#define	IDS_NUMDIAL_FILTER_SHARP	"Select desired filter sharpness"
	
#define	IDS_SCANNING_ROMS			"Scanning ROMs..."
	
#define	IDS_CHEAT_SEARCH_NEW		"New cheat search started"
#define	IDS_CHEAT_SEARCH_ADD_MATCH	"%i Addresses Matched"
#define	IDS_CHEAT_SEARCH_RESULTS	"Address %08X Value %02X"
#define	IDS_CHEAT_SEARCH_EXIT		"Cheat search exited"
	
#define	IDS_LANG_ENGLISH_US			"English (US)"
#define	IDS_LANG_SIMP_CHINESE		"Simplified Chinese"
#define	IDS_LANG_TRAD_CHINESE		"Traditional Chinese"
#define	IDS_LANG_JAPANESE			"Japanese"
#define	IDS_LANG_KOREAN				"Korean"
#define	IDS_LANG_FRENCH				"French"
#define	IDS_LANG_SPANISH			"Spanish"
#define	IDS_LANG_ITALIAN			"Italian"
#define	IDS_LANG_GERMAN				"German"
#define	IDS_LANG_PORTUGUESE			"Portuguese"
#define	IDS_LANG_POLISH				"Polish"
#define	IDS_LANG_HUNGARIAN			"Hungarian"
	
#define	IDS_CLONE_OF				" (clone of %s"
#define	IDS_ROMS_FROM_1				", uses ROMs from %s"
#define	IDS_ROMS_FROM_2				"%suses ROMs from %s"
#define	IDS_BOARD_ROMS_FROM			"%suses board-ROMs from %s"
#define	IDS_SAMPLES_FROM			"%suses samples from %s"
#define	IDS_NUM_PLAYERS				"%s%i player%s"
#define	IDS_NUM_PLAYERS_MAX			"s max"
#define	IDS_HARDWARE_DESC			"%s (%s, %s hardware)"
#define	IDS_MVS_CARTRIDGE			"Neo Geo MVS Cartidge"
#define	IDS_HISCORES_SUPPORTED		"%shigh scores supported"
#define	IDS_CARTRIDGE				"cartridge"
#define	IDS_HARDWARE				"hardware"
#define	IDS_EMPTY					"Empty"

#define	IDS_ERR_ERROR				0
#define IDS_ERR_WARNING				1
#define IDS_ERR_INFORMATION			2
#define IDS_ERR_UNKNOWN				3

#define	IDS_ERR_LOAD_OK				"The ROMset is OK."
#define	IDS_ERR_LOAD_PROBLEM		"The ROMset exhibits the following problems:"
#define	IDS_ERR_LOAD_NODATA			"No data was found, the game will not run!"
#define	IDS_ERR_LOAD_ESS_MISS		"essential data is missing, the game will not run!"
#define	IDS_ERR_LOAD_ESS_BAD		"essential data is invalid, the game might not run!"
#define	IDS_ERR_LOAD_DATA_MISS		"data is missing."
#define	IDS_ERR_LOAD_DATA_BAD		"data is invalid."
#define	IDS_ERR_LOAD_FOUND			"%.32hs was found (%.512s).\n"
#define	IDS_ERR_LOAD_NOTFOUND		"%.32hs was not found.\n"
#define	IDS_ERR_LOAD_INVALID		"The following ROMs are invalid:\n"
#define	IDS_ERR_LOAD_CRC			"%.32hs has a CRC of %.8X (correct is %.8X).\n"
#define	IDS_ERR_LOAD_SMALL			"%.32hs is %dk which is too small (correct is %dkB).\n"
#define	IDS_ERR_LOAD_LARGE			"%.32hs is %dk which is too large (correct is %dkB).\n"
#define	IDS_ERR_LOAD_DET_ESS		"essential "
#define	IDS_ERR_LOAD_DET_PRG		"program "
#define	IDS_ERR_LOAD_DET_BIOS		"BIOS "
#define	IDS_ERR_LOAD_DET_GRA		"graphics "
#define	IDS_ERR_LOAD_DET_SND		"sound "
#define	IDS_ERR_LOAD_DET_ROM		"ROM "
#define	IDS_ERR_LOAD_DISK			"Error reading %.32hs from %.512s"
#define	IDS_ERR_LOAD_DISK_CRC		"CRC mismatch reading %.32hs from %.512s"
#define	IDS_ERR_LOAD_REQUEST		"Error loading %.32hs, requested by %.128s.\nThe emulation will likely suffer problems."

#define	IDS_ERR_BURN_INIT			"There was a problem starting '%.128s'.\n"

#define	IDS_ERR_UI_NOSUPPORT		"%.32s is not supported by %.32s."
#define	IDS_ERR_UI_WINDOW			"Couldn't create main window."
#define	IDS_ERR_UI_MODULE			"Couldn't intialise '%s' module."
#define	IDS_ERR_UI_TRIPLE			"Couldn't create a triple buffer."
#define	IDS_ERR_UI_FULL_NOMODE		"No appropriate display mode supported. Please adjust your full-screen settings."
#define	IDS_ERR_UI_FULL_PROBLEM		"Problem setting '%dx%dx%dbpp (%dHz)' display mode"
#define	IDS_ERR_UI_FULL_CUSTRES		"\n\nThis is not a standard VGA resolution; please make sure it is supported by your video card."

#define	IDS_ERR_DISK_CREATE			"Error creating "
#define	IDS_ERR_DISK_LOAD			"Error loading "
#define	IDS_ERR_DISK_UNAVAIL		" is for an unavailable game."
#define	IDS_ERR_DISK_TOONEW			" does not work with this version of %s."
#define	IDS_ERR_DISK_TOOOLD			" requires a more recent version of %s, please upgrade."
	
#define	IDS_ERR_NO_NETPLAYDLL		"Error finding 'kailleraclient.dll'."	
#define	IDS_ERR_NO_DRIVER_SELECTED	"No driver has been selected from the list."	
#define	IDS_ERR_NO_DRIVER_SEL_SLOT1	"No driver has been selected for slot 1."
#define	IDS_ERR_NON_WORKING			"This game isn't working. Load it anyway?"
#define	IDS_ERR_CDEMU_INI_FAIL		"Couldn't initialise CD emulation."

#endif // _LOCALISE_H_

