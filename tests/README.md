# Return application tests

The host test compiles the production path validator, handoff policy and checked
raw SRAM writer without SDL, an emulator core or the Xbox SDK. From the repo root:

```sh
mkdir -p build/returnapp-tests
g++ -std=c++03 -Wall -Wextra -Werror -pedantic -Isrc tests/returnapp_test.cpp -o build/returnapp-tests/returnapp_test
build/returnapp-tests/returnapp_test build/returnapp-tests
```

It checks path syntax and length, unset configuration, missing targets, abort on
save failure, save/close/launch ordering, returned-launch failure, byte-for-byte
raw SRAM output and write/flush failure. The platform is replaced by a fake host;
these tests do not prove that a target XEX can run on hardware.

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

The worker drain, final Xbox `_commit`, actual core teardown, and XAM handoff need
these platform checks. A host test pass is not a console compatibility claim.
