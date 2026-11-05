# Return application tests

Run the host matrix on Linux/WSL with Python 3, GCC and a GNU-compatible linker.
No extra Python packages, SDL, emulator core or Xbox SDK are needed:

```sh
python3 tests/run_host_tests.py
python3 tests/run_host_tests.py --sanitize
```

All builds use `-Wall -Wextra -Werror -pedantic`. The sanitizer run enables address,
undefined behavior and leak checks. Logs and a machine-readable `results.json`
are written under `build/returnapp-tests` or `build/returnapp-sanitized`.

| Suite | Scope |
| --- | --- |
| Portable (C++03) | Actual `returnapp.h` and `rawsave.h`: path syntax, every extension case, 259/260-byte boundaries including UTF-8, all control bytes, a single-byte mutation sweep, unset/missing targets, handoff ordering, byte-for-byte SRAM and `/dev/full` write failure. |
| Xbox fakes (C++11 host) | 44 cases exercising selected production bodies with `_XBOX`, including the actual save drain, menu/hotkey dispatch, frontend loop, teardown orchestration and launch wrapper. |
| No-SRAM fakes (C++11 host) | 36 cases with `_XBOX` and `NO_SRAM`: drain pending work but skip SRAM queries/writes, alongside input and handoff behavior. |
| Windows fakes (C++11 host) | 28 cases selecting the non-Xbox preprocessor paths; configured targets do not add the Xbox menu action or change the exit-game hotkey. This is not a Windows application build. |

The application translation units include the XDK/SDL/D3D stack. To test their
current logic without restructuring production, the runner extracts selected
function bodies/sections and compiles them with controlled dependencies in
`production_exit_test.cpp`. Extraction fails if a definition is missing or
ambiguous, keeps source line directives, and records hashes/locations in the
results. Legacy source text encodings are normalized in the generated copies;
the repository sources are not rewritten by the runner.

The extracted code includes `saveSramForExit`, `closeGame`, `ReturnAppHost`,
`returnToConfiguredApp`, `runGameLoop`, `processActions`, the menu callback and
registration, the exit-game switch case, `Hotkeys` construction/dispatch, the
physical SDL button mapping, and `Launcher::launchReturnApp`. Tests cover:

- Idle/busy queues, completion near the deadline, five-second timeout, unsigned
  tick wrap, missing mutex and lock/unlock errors, no loaded ROM and no SRAM.
- A scripted older queued snapshot completing before the final SRAM write;
  it cannot overwrite the final bytes after the handoff proceeds.
- Real host file writes, with injected open/short-write/flush/commit/close errors;
  failed saves keep the core loaded and can be retried with current RAM.
- Back/Start defaults, modifier gating, cooldown, remapping, achievements guard,
  configured/unset menu registration, callback dispatch, and existing Return to
  game / Exit Salvia behavior. Handoff is deferred until after event callbacks.
- SRAM completion before core unload/deinit, no duplicate asynchronous save,
  audio/disc/cheat/MIDI/memory cleanup calls, cleared launch data, and one-shot
  missing-target/save/returned-launch failure paths. No core is unloaded twice.

The queue worker completes on a deterministic fake clock; this does not test
real SDL thread scheduling or prove race freedom. Rendering, device input, core
callbacks and Xbox APIs are stand-ins. In particular, the fake `_commit`, file
attributes, launch-data and launch functions do not validate the XDK signatures,
ABI, error behavior or successful console handoff. These are focused tests of
selected production logic, not a full application compile or hardware test.

Before taking the PR out of draft, build with the tools in [COMPILING.md](../COMPILING.md)
and verify on an Xbox 360 with a disposable test save and a separately installed
return application:

1. With `return_app` absent/empty, confirm Back + Start still opens Salvia's menu
   and there is no new menu item. Check the Windows build also keeps its behavior.
2. Set a valid absolute XEX path. Exit a game using both the exit-game hotkey and
   the menu action. Confirm the return app starts without receiving a ROM argument.
3. Change battery-backed game data, exit immediately while a prior SRAM/savestate
   write is active, then relaunch and verify the latest SRAM. Repeat on another
   core, a no-SRAM core, and with no game loaded. This does not create a savestate.
4. Try a nonexistent file, directory, malformed path and a disconnected USB drive.
   Verify an error appears in Salvia and the loaded game can still be resumed.
5. Simulate a stalled save worker or failed SRAM write. Verify no unload or handoff
   occurs and that another exit attempt can be made after fixing the failure.
6. Use a present but unloadable XEX. Verify a returned handoff reports an error,
   stays in Salvia's menu with the core unloaded, and can launch another game.
7. Exercise controller hotkey remapping and quitting with achievements enabled.
   The exit-game hotkey retains its existing achievements pause guard.

Actual save-worker timing, Xbox `_commit`, real core behavior and XAM handoff
still need these platform checks. A host test pass is not a console compatibility
claim.
