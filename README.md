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
The only thing needed to the emulators to work, is to provide the proper bios, as usual on every retroarch frontend. So the system directory should have the following files:

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
│   ├── blend/
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
│   ├── md_paprium.zip
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
    ├── artwork/
    ├── samples/
    └── hiscore.dat
```
### Roms
The config files come prepared to read the games from the directory Roms of the first usb (Usb0:\Roms) 
To get started, you simply need to place your backed-up games into their respective directories:

```
Usb0:\Roms
├── 3do\                 (3DO --> iso chd bin)
├── dos\                 (MS-DOS --> zip exe bat)
├── fbneo\               (Arcade FBNeo --> zip cue ccd chd)
│   ├── neocd\           (NeoGeo CD --> cue ccd chd)
│   ├── megadrive\       (FBNeo Megadrive core)
│   │   ├── paprium.zip  (Paprium game containing 'Paprium (World)(2020)(WaterMelon).bin')
├── gb\                  (Game boy --> zip gb)
├── gba\                 (Game boy advance --> gba zip)
├── Genesis\             (Megadrive/Genesis --> md bin zip)
├── gg\                  (GameGear --> gg zip)  
├── mame2003\            (M.A.M.E --> zip)
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
├── spectrum\            (FBNeo ZX Spectrum --> zip
```


