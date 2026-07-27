# PULSEWING

A procedural 3D rail flier controlled by a heartbeat. Pulse upward, dive through
the Sunfall ruins, chase perfect lines, fire through cracked barricades, and
roll through danger. One impact, one quarter-second bloom, one tap to fly again.

## Play

```sh
make
make run
```

Controls:

- `Space`, left click, or gamepad A: pulse
- `F`, right click, or gamepad X: fire
- `R` or gamepad B: roll
- `A/D`, arrows, or left stick: drift

Turrets now fire telegraphed magenta bolts. Roll through one to send it back,
raise the combo, and trigger a gold deflect bloom; shoot the turret first or
move off its lane to stay safe. Campaign flights carry three projectile hull
pips, while terrain and Endless/PURE remain uncompromising. Violet secret rings
open higher routes, boss stages reveal colossal fortresses, and progression
persists locally in `pulsewing.save`.

Campaign charts nine stages across three altitude tiers. Endless uses a stable
daily seed and ramps from 26 to 40 m/s. PURE removes every verb except pulse.
The full survival line never requires weapons or drift.

## Verify

```sh
make check      # determinism, collision, restart, reaction, contrast, comfort
make graybox    # the gameplay-first presentation
make web RAYLIB_WEB=/path/to/emscripten-raylib
```

There are no runtime assets. Geometry, stages, particles, sound effects, sky,
music language, and route data are generated from code.
