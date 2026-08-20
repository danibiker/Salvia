# CannonBall Libretro

CannonBall is an enhanced game engine for Sega's 1986 OutRun arcade
game. The original 68000 and Z80 code was rewritten in C++ by Chris
White, enabling enhancements such as smoother frame rates, widescreen
rendering and additional game modes.

This repository contains the Libretro core and synchronizes it with the
modern CannonBall engine while retaining the Libretro integration.

## Requirements

CannonBall requires legally obtained ROM files from the OutRun Revision
B arcade set. No copyrighted game data is included in this repository.

For instructions on downloading the CannonBall support files, arranging
the ROM files and starting the core, see:

https://docs.libretro.com/library/cannonball/

The core supports the `.game` and `.88` content entry points.

## Features

- original, 30, 60 and 120 FPS rendering modes
- widescreen and high-resolution rendering
- Continuous and Time Trial game modes
- analog steering, accelerator and brake controls
- force feedback on supported frontend and input-driver combinations
- custom track support
- optional custom music support
- additional car palette options
- Libretro core reset support
- persistent configuration and high scores
- Libretro core options and input remapping

Feature availability may depend on the Libretro frontend, operating
system and selected input driver.

## Building the Libretro core

The Libretro build uses `Makefile` and `Makefile.common`.

Run:

    make platform=<target> -j<number-of-jobs>

For example, on macOS:

    make platform=osx -j"$(sysctl -n hw.ncpu)"

The resulting core uses the platform-appropriate shared-library
extension, such as:

- `cannonball_libretro.so` on Linux and other Unix-like systems
- `cannonball_libretro.dylib` on macOS
- `cannonball_libretro.dll` on Windows

Available targets and platform-specific settings are defined in the
Makefile.

## Dependencies

The Libretro core includes the required Libretro common sources and
pugixml in the repository.

Configuration and high-score XML handling use pugixml. The previous
vendored Boost dependency is no longer required.

The Libretro build does not require SDL2. SDL2 and CMake are used by the
standalone CannonBall application instead.

## Standalone CannonBall

Documentation and source code for the standalone application are
available from the original project:

- Repository: https://github.com/djyt/cannonball
- Manual and wiki: https://github.com/djyt/cannonball/wiki
- Development blog: http://reassembler.blogspot.co.uk/

## Credits

- Chris White — CannonBall creator and original engine implementation
- Arun Horne — cross-platform work
- Libretro contributors — Libretro integration and maintenance
- CannonBall contributors — continued engine development

## License

CannonBall is distributed under a non-commercial license. See the
repository license files for the complete terms.

The original OutRun ROM files and other copyrighted game data are not
included.
