# hidmouse.xex — Resident USB HID mouse plugin for Salvia (Xbox 360)

DashLaunch plugin that reads a USB HID mouse (boot protocol) by hooking into the kernel's USB stack and publishes the raw deltas (X/Y/wheel + buttons) in a global struct. Salvia only reads this struct and pushes the events to SDL.

Because the plugin is resident (loaded by DashLaunch at boot, never unloaded), the mouse survives Salvia's `XLaunchNewImage` (core swap): without crashing or replugging — which was the problem with doing it inside Salvia's `.xex` file.

Requirements: CFW console (RGH/JTAG), dashboard build **17559** (retail) or **17489** (devkit). Other builds will not install (fail-safe). Add its addresses to `g_builds[]` in `main.cpp`.

## Files

- `main.cpp` — driver (USB hooks + interrupt callback + publish to `g_hidMouseShared`).

- `HidMouseShared.h` — shared contract (struct + magic + VA).

- `Detours.h` — PPC detour (iMoD1998).

- `xex.xml` — XEX configuration (**sysdll**, base `0x81F00000`).

- `hidmouse.vcxproj` — project (DynamicLibrary `/dll`, XDK VS2010).

## Compile

1. Open `hidmouse.vcxproj` with Visual Studio + Xbox 360 XDK (toolset 2010).

2. You need `xextool.exe` in this folder (copy it from `x360remap\hiddriver` or your XDK) for post-build (`-r a -m r`, sets privileges for `.xex`).

3. `xkelib` is not needed (we resolve the kernel by ordinal with `GetProcAddress`). If your environment requires `xkelib` to build a sysdll, copy that folder from x360remap
and add it to the project's include/lib.

4. Compile → `hidmouse.xex`.

## Channel location — NO adjustments (no `.map`)

No need to look up any addresses or touch constants. The plugin is a sysdll with a fixed base value of `0x81F00000` (xex.xml), and Salvia scans that image looking for the struct signature (`magic` + `version`). This allows it to find `g_hidMouseShared` wherever it is located within the plugin image, and it works even if you recompile the plugin and change the layout. If the plugin isn't loaded, the scan (protected with SEH) simply won't find the signature, resulting in no operation and no crash.

> Note: If you ever change `baseaddr` in `xex.xml`, update `HIDMOUSE_PLUGIN_BASE` in `HidMouseShared.h` and in the Salvia reader (and `HIDMOUSE_SHARED_VERSION` if the struct layout changes). With the default base, nothing needs to be changed.

## Deployment

1. Copy `hidmouse.xex` to `Hdd1:\` (or wherever your plugins are located).

2. In **DashLaunch**, add it as a plugin (`plugin1`…`plugin5` = path to `.xex`) and save.

3. **Restart the console** (plugins load on startup).

4. Connect the mouse (if it was connected before startup, reconnect it: there is a ~15-second blackout window at startup to prevent it from claiming during boot enumeration).

## Testing

- In Salvia, move the mouse → it responds in the menu.

- **Launch cores and return, several times, without touching the mouse** → the mouse continues to work (the plugin is resident), without crashing or replugging. This is the key test.

- Without the plugin loaded → Salvia starts normally and the mouse does not work (fail-safe).
- Debug: Add `HIDMOUSE_PLUGIN_DEBUG 1` to `main.cpp` for traces via `OutputDebugStringA` (visible via XDBDM).
