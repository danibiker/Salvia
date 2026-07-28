# Salvia
I had grown weary of juggling a multitude of different emulators, each requiring its own convoluted hotkey configuration just to navigate or simply exit the application. Consequently, I resolved to build my own frontend from scratch, leveraging the exceptional capabilities of various libretro cores. Compiling each individual core proved to be a formidable challenge in its own right, and developing the frontend itself was by no means a straightforward task. Nevertheless, through sheer perseverance—coupled with invaluable assistance from Claude and Open Code, I managed to overcome the vast majority of the hurdles and technical quirks I encountered. I sincerely hope you enjoy this frontend, as it is fully capable of providing countless hours of entertainment through some of the most influential consoles in history.

**Salvia** is a simple but powerfull frontend for libretro emulators mainly focused to run properly on the Xbox 360, but also able to run on Windows. 

<img width="1282" height="747" alt="image" src="https://github.com/user-attachments/assets/baa0d8c4-34ee-46cc-84bc-845ba1cb0a41" />
<p align="center"><em>Main fronted</em></p>
<img width="1282" height="747" alt="image" src="https://github.com/user-attachments/assets/3d19e401-4f18-4ae9-9b9b-2f73a4347a65" />
<p align="center"><em>Filtering games</em></p>
<img width="1282" height="747" alt="image" src="https://github.com/user-attachments/assets/7bad84c7-a921-4139-99bc-c09c75f8090c" />
<p align="center"><em>Configuration menu</em></p>

## Features
* Integration with **retroachievements**
* Graphic **filters** (Nearest, Sharp bilinear, LCD3x, scanlines, CRT-Geom, CRT-Lottes, CRT-EasyMode, HQ2X, HQ3X, HQ4X, XBR, 5XBR)
* **Integer scaling** (reduced or increased scale)
* Different **aspect ratios**
* **Savestates**
* Image and description **scraper** from screenscraper.fr
* **F.A.Q and Walktrough** viewer from gamefaqs.gamespot.com
* **Fast forward**
* **Navigate zipped files** and load contained games directly (they need to have a symbol @ as the first letter to be opened)
* **Disc control** to change disks on PSX, SegaCD and PC Engine CD
* **Bios Boot** (To organize PSX memory card savegames)
* **Buttons mapper** for each controller
* Ingame **Hotkeys**
* **Animated** or static frontend **backgrounds**
* Game **library search filters** (for FBNeo, FBANext and Mame 2003 plus)
* **Per-core** libretro configuration

## Emulators
Salvia provides the following emulators from the latests releases:

* Megadrive/Genesis/Sega CD
  - genesis-plus-gx
  - picodrive
* Super Nintendo/Super Famicom
  - snes9x
  - snes9x2010
* Master System/Game Gear/SG-1000
  - genesis-plus-gx
* Nes/Famicom
  - nestopia
* GameBoy/GameBoy Color
  - gambatte
* Game Boy Advance
  - vba-next
* PC Engine/Turbografx-16/PC Engine CD
  - beetle-pce
  - beetle-pce-fast
* PC Engine SuperGrafx
  - beetle-supergrafx
* Arcade
  - FBNeo
  - FBANext (ported from the magicseb repository)
  - mame-2003-plus
* Play Station 1
  - pcsxr-360 (ported to libretro from the Wolf3s repository)
* MS-DOS
  - dosbox-pure (with dynamic powerpc recompiler working)
* 3DO
  - 3dox (ported to libretro from the Lantus version -> "3dox - Xbox 360 New Years Day Pre-Release - V0.03")
* Quake
  - tyrquake
* Doom
  - prboom

## Configuration

### Salvia Frontend directory
Salvia can be installed into any directory (HDD or USB). As a suggestion, I like to leave it in USB0:\Salvia

<img width="694" height="527" alt="image" src="https://github.com/user-attachments/assets/2e667993-959c-4f28-8117-97db79dd522b" />

### Bios
The only thing needed to the emulators to work, is to provide the proper bios, as usual on every retroarch frontend. All the files needed can be downloaded from [https://github.com/Abdess/retrobios](https://github.com/Abdess/retrobios/releases/download/v2026.04.02/RetroPie_v1.22.2_Platform_BIOS_Pack.zip).
The system directory should have the following files:

```
system/
├── 3do_arcade_saot.bin
├── 3do_devkit_1.0fc2.bin
├── bios_CD_E.bin
├── bios_CD_J.bin
├── bios_CD_U.bin
├── bios_MD.bin
├── gba_bios.bin
├── goldstar.bin
├── NstDatabase.xml
├── panafz1-kanji.bin
├── panafz1.bin
├── panafz10-norsa.bin
├── panafz10-patched.bin
├── panafz10.bin
├── panafz10e-anvil-norsa.bin
├── panafz10e-anvil-patched.bin
├── panafz10e-anvil.bin
├── panafz10ja-anvil-kanji.bin
├── panafz1j-kanji.bin
├── panafz1j-norsa.bin
├── panafz1j.bin
├── prboom.wad
├── sanyotry.bin
├── SCPH1001.BIN
├── scph101.bin
├── scph5500.bin
├── scph5501.bin
├── scph5502.bin
├── scph7001.bin
├── scph7502.bin
├── syscard1.pce
├── syscard2.pce
├── syscard3.pce
├── fbneo/
│   ├── blend/ (Not needed really but nice to have)
│   │   ├── 1941.bld ... zupapan.bld (110 .bld files)
│   │   └── ...
│   ├── cheats/
│   │   └── cheat.dat
│   ├── samples/
│   │   ├── paprium/ (52 .wav files)
│   │   ├── blockade.zip ... zerohour.zip (samples .zip files)
│   │   └── ...
│   ├── bubsys.zip
│   ├── cchip.zip
│   ├── channelf.zip
│   ├── cnebula.zip
│   ├── coleco.zip
│   ├── decocass.zip
│   ├── fdsbios.zip
│   ├── hiscore.dat
│   ├── isgsm.zip
│   ├── midssio.zip
│   ├── msx.zip
│   ├── namcoc69.zip
│   ├── namcoc70.zip
│   ├── namcoc75.zip
│   ├── neocdz.zip
│   ├── neogeo.zip
│   ├── ngp.zip
│   ├── nmk004.zip
│   ├── pgm.zip
│   ├── skns.zip
│   ├── spec128.zip
│   ├── spec1282a.zip
│   ├── spectrum.zip
│   └── ym2608.zip
└── mame2003-plus/
│   ├── artwork/
│   ├── samples/
│   └── hiscore.dat
└── patches/
    └── psx/
       └── SCES_003.11.sbi ... SLES_329.69.sbi (197 .sbi files)

```
### Roms
The config files come prepared to read the games from the directory Roms of **the first usb (Usb0:\Roms)**
To get started, you simply need to place your backed-up games into their respective directories:

```
Usb0:\Roms
├── 3do\                 (3DO --> iso chd bin)
├── dos\                 (MS-DOS --> zip exe bat)
├── fbneo\               (Arcade FBNeo --> zip cue ccd chd)
│   ├── neocd\           (NeoGeo CD --> cue ccd chd)
│   ├── megadrive\       (FBNeo Megadrive core)
│       └── paprium.zip  (Paprium game containing 'Paprium (World)(2020)(WaterMelon).bin')
├── gb\                  (Game boy --> zip gb)
├── gba\                 (Game boy advance --> gba zip)
├── Genesis\             (Megadrive/Genesis --> md bin zip)
├── gg\                  (GameGear --> gg zip)  
├── mame2003\            (M.A.M.E --> zip)
│   └── roms\            
│   └── samples\         
├── megacd\              (Sega CD --> md bin chd zip)
├── msx\                 (FBNeo MSX --> zip)
├── nes\                 (Nintendo Nes/Famicom --> nes zip)
├── ngp\                 (FBNeo NeoGeo Pocket --> zip
├── pce\                 (PcEngine/Turbografx16 --> zip pce)
├── pcecd\               (PcEngineCD/TurbografxCD --> zip pce chd)
├── pcfx\                (PCFX/Supergrafx --> zip pce)
├── prboom\              (Doom Engine --> wad)
├── psx\                 (Play Station 1 --> iso chd bin cue m3u)
├── quake\               (Quake 1 Engine -> pak)
├── sms\                 (Master System/SG-1000 --> sms zip)
├── snes\                (Super Nintendo/Super Famicom --> zip sfc)
└── spectrum\            (FBNeo ZX Spectrum --> zip)
```

If you want to change this routes, they should be changed manually editing the field **rom_directory** form the corresponding .cfg file of the config subdirectory

### Hotkeys
Hotkeys can be accessed via the menu: Options > Input > Hotkeys. By default, the SELECT button acts as the primary hotkey enabler and must be pressed first, followed by the corresponding function button to execute the desired action. For instance, the 'SELECT + START' combination exits the emulation, returning you to the game selection screen. Another essential shortcut is 'SELECT + Y', which brings up an overlay containing the options menu or returns to the game emulation if pressed again. The remaining options are fairly self-explanatory.

<img width="1279" height="381" alt="image" src="https://github.com/user-attachments/assets/1cdfb429-d4f4-40ef-baf9-d13c7808c39e" />


## Core Configurations

### PSX
The psx core needs some .sbi files to work properly with certain PAL titles. If your game hangs or is unable to boot, it's likely to need it. The .sbi files must be copied into the directory system\patches\psx, or alternatively, renamed with the same name as the cd file loaded, but mantaining the .sbi extension in the same directory.

This files can be downloaded from [psxdatacenter.com](https://psxdatacenter.com/sbifiles.html). You can use the Firefox plugin [DownThemAll](https://www.downthemall.net/) to download them easily:

<img width="985" height="458" alt="image" src="https://github.com/user-attachments/assets/782e1da3-6ca9-492e-a2e2-2b3d5e7daf8e" />



Certain games are only compatible with a DualShock controller. If required by the title, this can be configured directly within the Options menu:
Options > Input > Retropad assignments > Port Controller 1 > Joystick type

<img width="1279" height="276" alt="image" src="https://github.com/user-attachments/assets/ce5e9684-40be-452d-8cf4-2a0ce7ded53f" />

### DOSBOX-PURE
This core can load games from a compressed .zip file, but if the game is too big or demanding, it can introduce slowdowns. For this games (like Duke Nukem 3D) it's better to load them uncompressed from a directory

It also may be required a specific joystick type. If the game fails to respond to your controller, adjust the settings within the Options menu:
Options > Input > Retropad assignments > Port Controller 1 > Joystick type

<img width="1279" height="275" alt="image" src="https://github.com/user-attachments/assets/15fcf292-7c27-466e-b28a-1c9aa030ae42" />

This core features a built-in virtual keyboard, which is activated by pressing the L3 stick by default. Once the window appears, you can cycle through the various options using the L and R buttons. This interface allows you to both use the keyboard and map or modify specific keys to the controller buttons.

<img width="1278" height="721" alt="image" src="https://github.com/user-attachments/assets/4f5a428b-fc2f-46d2-8ed6-3debee99cdbe" />

### FBNEO
For this core, there are two subdirectories available **neocd** (to load neogeo cd games) and  **megadrive** (it can load megadrive games for the fbneo core, but its main purpose is to load the game Paprium as Genesis-plus-gx is the gold standard for megadrive)

Some games should run fullspeed but the fbneo team introduced some changes that make them slower than it should be (Altered Beast for example). For these game, use the alternative emulator FBANext

#### Paprium
To run this game, the following files must be present. Additionally, upon launching the game for the first time, you will be prompted to select your preferred language. Once selected, the game requires a restart, and this preference will be saved for future sessions.

The files needed must be placed in the following directories. Take into account that the music files are usually in mp3 format. To avoid slowdowns and reduce the memory footprint for Xbox 360, convert them to wav files. I recommend this portable conversion tool [fre:ac Portable](https://portableapps.com/apps/music_video/freac_portable)

```
system
└── fbneo/
    └── samples/
        └── paprium/ (01 Theme of Paprium.wav ... 52 Waterfront Beat.wav --> 52 .wav files)
```
```
Usb0:\Roms
└── fbneo\               (Arcade FBNeo --> zip cue ccd chd)
    └── megadrive\       (FBNeo Megadrive core)
        └── paprium.zip  (Paprium game containing 'Paprium (World)(2020)(WaterMelon).bin')
```
#### Other FBNEO cores
The MSX, ZX-Spectrum and NeoGeo Pocket rely on FBNEO. Search a compatible romset for these cores.
For MSX and ZX Spectrum cores, an on-screen overlay keyboard is available and can be enabled by pressing the default shortcut SELECT + X

<img width="1281" height="722" alt="image" src="https://github.com/user-attachments/assets/01e73cc0-96b4-4e67-82e5-b78738a58989" />


### QUAKE
To load the right episode or mod of quake, a subdirectory must be created for each of them. The structure should be as follows:
```
Usb0:\Roms
└── quake\
    ├── id1\ 
    │   ├── music
    │   │   └── track02.ogg ... track11.ogg (10 .ogg files)
    │   ├── pak0.pak
    │   └── pak1.pak
    └── hipnotic\ 
        ├── music
        │   └── track02.ogg ... track09.ogg (8 .ogg files)
        └── pak0.pak
```
### DOOM
To load the right episode or mod of Doom, the wad files should be copied on the prboom directory. The structure should be as follows:
```
Usb0:\Roms
└── prboom\
    ├── DOOM.WAD
    ├── DOOM2.WAD
    ├── PLUTONIA.WAD
    ├── SIGIL.WAD
    └── TNT.WAD
```
## Games artwork
If you don't want to use the builtin scraper, you can add your images and text for each game manually. The images must be in png format and the description must be a .txt file. To be able to do that, you must create this directory structure for the emulator. For example, to add images and text descriptions for the mame core:

```
Usb0:\Salvia\assets\mame
├── box2d\ (An image of the game box cartridge)
|   └── 3countb.png ... zzyzzyxx.png (2919 .png files)
├── snap\ (An ingame screenshot)
|   └── 3countb.png ... zzyzzyxx.png (2919 .png files)
├── snaptit\ (A game's title screenshot)
|   └── 3countb.png ... zzyzzyxx.png (2919 .png files)
└── synopsis\
    └── 1on1gov.txt ... zzyzzyxx2.txt (37751 .txt files)
```

The images can be easily found on internet, but to generate the synopsys directory files, a script is provided in the utils\extract_history.ps1 git main path. To execute it, edit its content and modify the $baseDir line with a directory path containing the MAME "history.xml" file. Open a "Windows power shell" and execute it. 

<img width="591" height="158" alt="image" src="https://github.com/user-attachments/assets/3676f16c-d8fe-4a1a-9768-fd666ea002e3" />

### Games titles
Some emulators like MAME or Final Burn Neo/Alpha, have short rom filenames that make really difficult to differenciate which game is which, so, to translate them, some files are prepared in the main distribution of the salvia frontend. The location is the following:

```
Usb0:\Salvia\assets\extra
├── merged_mame.xml
├── merged_fbneo.xml
└── merged_ngp_spectrum_msx.xml
```
Furthermore, each file must be referentiated into the mame_roms_xml property of the .cfg file for each emulator 

```
mame_roms_xml = assets\extra\merged_mame.xml
```

if you want manually generate the titles for each game, a script is provided in the utils\merge_mame_files.ps1 git main path. This script needs the .dat or .xml files provided for MAME or FBNEO, and generates a reduced xml with the important information extracted. For example, the merged-* files described above where generated with the following files

- fbneo.dat
- MAME 0.284.dat
- mame2003-plus.xml

<img width="655" height="230" alt="image" src="https://github.com/user-attachments/assets/ee3a2e89-5134-4509-b290-085b1c0be87e" />


# Compiling
[see COMPILING.md](COMPILING.md)
