# Songs From Bad Sectors

Songs From Bad Sectors is a two-to-three-minute immersive 3D audiovisual arcade
game written entirely in C99. Every six-character seed generates a song, disk
world, rhythm patterns, color palette, and corruption path. Explore the disk in
first person or switch to an elevated 3D overview. All geometry, effects, and
audio are created at runtime; the game ships with no recorded music or textures.

## Quick start on macOS

Install the build tools and raylib 6.0 with Homebrew:

```sh
brew install cmake raylib
```

From the repository root, configure and build the playable game:

```sh
cmake -S . -B build-game \
  -DSFBS_BUILD_GAME=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix raylib)"
cmake --build build-game -j
```

Launch it with:

```sh
./build-game/SongsFromBadSectors
```

The first CMake command only needs to be repeated when build options or
dependencies change. After editing source files, rebuild and play with:

```sh
cmake --build build-game -j
./build-game/SongsFromBadSectors
```

If CMake says that raylib was not found, verify `brew list raylib` reports 6.0
and keep the `CMAKE_PREFIX_PATH` option shown above.

## How to play

You are the bright read head inside a damaged disk. Glowing sectors around the
three rings are recoverable pieces of the generated song. Move through them to
restore the track before the 56-bar read finishes.

Your movement is recorded. After a short delay, the dark corruption ribbon
retraces that old route and makes it dangerous. The main decision is therefore
not simply which bright sector to collect, but where to travel without boxing
your future self into the returning echo.

The run develops in stages (the rhythm section now begins building during the
first Signal phrase, so the arrangement changes well before the midpoint):

1. The opening phrases introduce movement and sector recovery without damage.
2. Percussion enters and the dark echo begins following your previous route.
3. Bass and denser ring patterns create more routing choices.
4. The lead and full visual bloom arrive late in the track.
5. Final Read provides one last recovery pattern and resolves the song.

Recovery is the only score. Each recovered sector adds its weighted value to
the percentage in the upper-left corner. A sector darkened by corruption loses
its value until repaired again. The result screen rates the final percentage
from Signal Fragment through No Bad Sectors.

### Pulse

Pulse is the single action button and always works. It:

- gives the read head a burst of speed in the held direction;
- uses current movement direction when no direction is held;
- collects or repairs sectors in a larger radius;
- grants a brief moment of protection from the corruption ribbon.

Pulsing near a musical beat produces a stronger burst and larger repair radius,
but there are no misses or timing grades. Off-beat Pulse is still useful. The
small arc around the player shows its cooldown.

### Damage and integrity

Touching the dark ribbon corrupts a recently recovered sector, dulls the music,
pushes the player away, and removes integrity in Flow or Pulse. The small arcs
around the center show remaining integrity. A short invulnerability window
prevents one contact from dealing repeated damage.

Bloom has no early game-over. Flow begins with three integrity. Pulse begins
with two. A failed run still resolves musically and shows the recovery result.

### Modes

- **Bloom:** Relaxed mode. No early failure, a longer echo delay, a narrower
  ribbon, and a stronger Pulse. Use this to learn the game or enjoy the song.
- **Flow:** Default mode. Three integrity and the intended balance of recovery
  and route planning.
- **Pulse:** Challenge mode. Two integrity, a shorter delay, wider ribbon,
  denser late patterns, and slightly faster movement.

The same seed generates the same song, palette, phrases, and node layout in
every run. Mode only changes pressure and forgiveness.

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Move (overview) | WASD or Arrow Keys | Left stick or D-pad |
| Drive / turn (first person) | W/S forward/back, A/D turn | Left stick forward/back and turn |
| Pulse | Space, Z, or Enter | South face button or right trigger |
| Confirm | Enter or Space | South face button |
| Back | Escape | East face button |
| Pause/resume | Escape or P | Menu/Start |
| Restart from pause/results | R | Use the on-screen result action |
| Random seed on seed screen | R | Cycle the six slots with D-pad |
| Volume | Minus / Equals | — |
| Toggle fullscreen | F11 | — |
| Change language | L on menus | North face button on menus |
| Change 3D view | V | Right-stick click |
| Exit from title | Q or Escape | East face button |

On the seed screen, keyboard input ignores ambiguous `0`, `O`, `1`, `I`, and
`L`. With a gamepad, left/right selects a slot and up/down cycles its character.

The interface supports English, Spanish, French, German, Korean, Japanese, and
Chinese. Korean, Japanese, and Chinese use ASCII romanization so every glyph is
available in the game's embedded, asset-free font. Press L on the title, mode,
pause, results, or credits screen to cycle languages. The gamepad north face
button does the same. The selected language is saved automatically.

Gameplay starts in the fixed, north-up 3D arcade view. Movement is directly
screen-aligned: up always moves toward the top of the disk. A target beacon and
the upper-right radar point toward the nearest recoverable sector; the dark radar
marker and `ECHO` readout show the pursuing corruption path. The lower-left HUD
shows when Pulse is ready.

Press V or click the right stick to switch to first person. In first person,
W/S or the left stick moves forward and backward; A/D or left-stick left/right
turns the camera in the same direction. The mouse or right stick also looks
around. There is no left/right strafing in this view. Press V again whenever you
need the clearer full-disk view. The selected camera mode is saved.

## Menus and replay

From the title screen:

- Enter starts mode selection.
- S opens seed entry.
- C opens credits.
- Q or Escape exits.

The results screen supports R or Enter to replay the same seed, N for a new
random seed, M to change mode, S to enter a seed, and Escape to return to title.
The last seed, selected mode, volume, fullscreen preference, and tutorial state
are stored in `sfbs.sav`. A missing or unwritable save never prevents play.

## Debug build and controls

Development builds enable diagnostic controls by default:

```sh
cmake -S . -B build-debug \
  -DSFBS_BUILD_GAME=ON \
  -DSFBS_DEBUG_TOOLS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$(brew --prefix raylib)"
cmake --build build-debug -j
./build-debug/SongsFromBadSectors
```

- F1: transport and game-state overlay
- F2: collision-width view
- F3: freeze corruption
- F4: palette gallery strip
- F5: force damage
- F6: recover all currently visible sectors
- F7: jump to the next arrangement phase
- F8: regenerate and start a new seed
- 1–4: toggle pad, percussion, bass, and lead

These controls are compiled out when `SFBS_DEBUG_TOOLS=OFF`.

## Automated tests

The deterministic core builds without raylib:

```sh
cmake -S . -B build -DSFBS_BUILD_GAME=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/sfbs_validate 100000
```

Run the complete 56-bar simulation in Bloom, Flow, and Pulse without opening a
window or audio device, and emit the strict machine-readable status object:

```sh
./build/sfbs_status
```

The playable executable exposes the same early-exit mode when raylib is built:

```sh
./build-game/SongsFromBadSectors --headless-test
./build-game/SongsFromBadSectors --status-json
./build-game/SongsFromBadSectors --validate
./build-game/SongsFromBadSectors --control-test
```

Each command writes exactly one JSON object to stdout and returns nonzero when a
mechanically testable requirement fails. The report includes build state, check
and failure counts, deterministic digest, completed modes, feature flags, and
the remaining device-specific review boundaries.

`--control-test` runs the scripted control harness. It switches through Bloom,
Flow, and Pulse; drives deterministic movement and Pulse input; verifies pause
and resume, complete results, early failure, Bloom survival, same-seed restart,
new-seed play, and title return; and emits its own strict JSON result. The same
harness is also available without raylib as `./build/sfbs_control_harness`.

When raylib is available, configure with `SFBS_BUILD_GAME=ON` to include the
offline runtime-audio integration test. The operator playthrough checklist is
in `E2E_TEST.md`.

## Windows release

The contest submission target is a statically linked Windows x64 executable.
Use pinned raylib 6.0 and MinGW-w64 with the appropriate CMake toolchain. The
final staging directory should contain only:

```text
SongsFromBadSectors.exe
README.txt
```

Check the exact extracted size before packaging:

```sh
cmake -DSTAGE=path/to/stage -P scripts/check_size.cmake
```

Or create a clean two-file stage, enforce the size gate, and produce the archive
in one command:

```sh
cmake -DEXECUTABLE=build-release/SongsFromBadSectors.exe \
  -DOUTPUT=dist -P scripts/package.cmake
```

The checker fails above 1,474,560 bytes and warns above the 1,350,000-byte
safety target. Final release approval requires clean Windows 10 and Windows 11,
GPU, audio-device, and XInput testing.

## License notice

Project source is GPL-3.0; see `LICENSE`. Built releases use raylib, Copyright
(c) 2013-2026 Ramon Santamaria and contributors, under the zlib/libpng license.
The raylib license notice must be included in the shipped `README.txt`.
