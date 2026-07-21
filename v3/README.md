# POLYBLOOM

POLYBLOOM is a compact C99/Raylib 3D platformer built entirely from generated
geometry and synthesized audio. The design source of truth is
`POLYBLOOM_Complete_Game_Requirements_and_Design_Spec.md`.

## Development build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/polybloom
```

## Size-focused release build

```sh
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DPOLYBLOOM_RELEASE=ON
cmake --build build-release
```

The post-build check reports extracted package size and fails if the executable
exceeds 1,474,560 bytes. The build prefers a static raylib archive so the result
does not depend on a separately packaged raylib library.

