# Songs From Bad Sectors

A deterministic audiovisual arcade game in C99. A six-character seed creates a
56-bar song, Euclidean disk arena, palette, and delayed-path corruption hazard.
No textures or recorded audio are used.

## Build and test

raylib 6.0 must be installed or supplied through its CMake package. The core and
all headless tooling intentionally build without raylib:

```sh
cmake -S . -B build -DSFBS_BUILD_GAME=OFF
cmake --build build
ctest --test-dir build --output-on-failure
./build/sfbs_validate 100000
```

For the game, configure with `-DSFBS_BUILD_GAME=ON`. Controls are WASD/arrows or
gamepad stick/D-pad, Space/Z/Enter or south button to Pulse, Escape/P to pause,
and R to restart. Debug builds expose F1 overlay, F2 collision view, F3 echo
freeze, F4 gallery, F5 damage, F6 full recovery, F7 phase jump, F8 regenerate,
and number keys for stem muting.

## Windows release

Use a pinned static raylib 6.0 build with MinGW-w64 and configure CMake with the
matching toolchain. Recommended release flags begin with `-Os -flto
-ffunction-sections -fdata-sections` and linker garbage collection. Stage only:

```text
SongsFromBadSectors.exe
README.txt
```

Then enforce the extracted-byte limit:

```sh
cmake -DSTAGE=path/to/stage -P scripts/check_size.cmake
```

The checker fails over 1,474,560 bytes and warns over 1,350,000 bytes. A final
release still requires clean Windows 10/11, GPU, audio-device, and XInput tests.

The operator playthrough checklist is in `E2E_TEST.md`.

## License notice

Project source is GPL-3.0; see `LICENSE`. Built releases use raylib, Copyright
(c) 2013-2026 Ramon Santamaria and contributors, under the zlib/libpng license.
The raylib license notice must be included in the shipped `README.txt`.
