# POLYBLOOM

POLYBLOOM is a compact C99/Raylib 3D platformer built entirely from generated
geometry and synthesized audio. The design source of truth is
`POLYBLOOM_Complete_Game_Requirements_and_Design_Spec.md`.

## Standard size-safe build

```sh
cmake -S . -B build
cmake --build build
./build/polybloom
```

This default build is optimized, statically links Raylib when its archive is
available, and fails if the result exceeds 1,474,560 bytes.

## Development build with diagnostics

```sh
cmake -S . -B build-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPOLYBLOOM_RELEASE=OFF
cmake --build build-debug
```

The post-build check reports extracted package size and fails if the executable
exceeds 1,474,560 bytes. The build prefers a static raylib archive so the result
does not depend on a separately packaged raylib library.

## Controls

- Move: WASD, arrow keys, controller left stick, or D-pad
- Camera: automatic follow; mouse or right stick temporarily orbits
- Jump / Glide: Space or controller A
- Burst: Left Shift, J, or controller X
- Recenter camera: R or right shoulder
- Pause: Escape or Start
- Restart: hold Backspace or both shoulder buttons for 0.5 seconds
