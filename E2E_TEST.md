# Operator E2E acceptance check

Build and launch:

```sh
cmake -S . -B build-release -DSFBS_BUILD_GAME=ON -DSFBS_DEBUG_TOOLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
./build-release/SongsFromBadSectors
```

Please report the first failed numbered item with the seed and mode.

1. Boot reaches the title in under three seconds and can be skipped.
2. Title shows seed, mode, volume, controls, credits, fullscreen, and exit choices.
3. Seed entry ignores `0`, `O`, `1`, `I`, and `L`; random seed and six-slot entry work.
4. Bloom completes without early game-over; Flow has three integrity; Pulse has two.
5. WASD/arrows and gamepad stick/D-pad move smoothly; Space/Z/Enter, south button,
   and right trigger Pulse. Pulse never fails and feels stronger near the beat.
6. In a new profile, the short movement, Pulse, and echo tutorial prompts appear.
7. Nodes telegraph, become collectible without exact timing, and match audible rhythm.
8. The dark ribbon clearly follows the prior player route and contact cannot rapidly
   remove multiple integrity points.
9. Music begins with pad, then adds percussion, bass, and full lead as the run grows.
   A hit temporarily dulls the mix without clicks; audio and sweep remain aligned.
10. Escape/P pauses both play and transport. Resume does not jump or desynchronize.
11. Failure leaves a sparse pad resolution. Completion reaches a distinct Final Read.
12. Results show seed, mode, recovery, and correct result label. Replay, new seed,
    mode selection, seed entry, and title return all work.
13. Volume and F11 fullscreen changes persist after relaunch. Save failure is nonfatal.
14. Complete one run at 60 Hz and one at high refresh with no visible timing change.
15. Complete one run muted and verify corruption and node states remain understandable.

Hardware release checks still required separately: Windows 10 x64, Windows 11 x64,
integrated graphics, XInput controller, focus loss, and a clean directory with no DLLs.
