# COMPILING
To compile salvia, three .bat files are provided to generate the libraries and final executables. The order should be the following:

```
Salvia
├── 1. .\libs\rebuildAllWin.bat and libs\rebuildAllXbox.bat (Compile common libraries for salvia)
├── 2. .\libretro\rebuildAll.bat (Compile each libretro core into an independent library)
└── 3. .\rebuildAllWin.bat and rebuildAllXbox.bat (Link each core and integrate them into the salvia frontend)
```

## REQUISITES
IDE and libraries needed:
- Visual Studio 2010 and Service pack 1
- Xbox 360 Extensions for Visual Studio 2.0.21256.0

## WHY SO MANY XEX'S?
Due to the architecture of xbox 360 each core can't be compiled into a dynamic library and loaded in runtime, so each .xex file integrates the frontend and each core code.
