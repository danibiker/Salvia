# Salvia
**Salvia** is a simple but powerfull frontend for libretro emulators mainly focused to run properly on the Xbox 360, but also able to run on Windows. 



It provides the following emulators from the latests releases:

* Megadrive/Genesis
  - genesis-plus-gx
  - picodrive
* Super Nintendo/Super Famicom
  - snes9x
  - snes9x2010
* Master System & Game Gear
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
