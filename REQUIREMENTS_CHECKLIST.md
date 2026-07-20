# Songs From Bad Sectors — implementation checklist

This checklist traces `REQ_DESIGN.md` v1.0. A checked item is implemented in the
source tree; platform acceptance items still require the documented Windows test.

## Product and flow

- [x] BOOT, TITLE, SEED_SELECT, MODE_SELECT, PLAYING, PAUSED, RESULTS, CREDITS, EXIT
- [x] Six-character normalized seed, random seed, same-seed replay, three modes
- [x] 56-bar / 14-phrase run: Boot 4, Signal 8, Pulse 12, Flow 12, Bloom 12, Final 8
- [x] Recovery result labels, graceful early failure, restart and pause
- [x] Keyboard and gamepad mappings; short in-world tutorial prompts
- [x] Minimal HUD: recovery, integrity arcs, seed, pulse recharge

## Deterministic game systems

- [x] Independent music/layout/gameplay/visual PRNG streams (FR-010–014)
- [x] Curated genome: BPM, mode, root, progression, palette, density and warmth
- [x] Euclidean E(k,n), rotations, density guards and non-repeating adjacent phrases
- [x] Three rings, faint anchors, six node states, weighted recovery (FR-030–039)
- [x] Inertial fixed-step movement, soft circular boundary and always-valid Pulse
- [x] Continuous Gaussian beat resonance and mode-specific cooldown/impulse
- [x] Musical-time path buffer, delayed beat-stepped echo, ribbon collision and i-frames
- [x] Bloom/Flow/Pulse pressure, integrity, recent-node corruption and route pushback
- [x] Final Read combines earlier patterns; success/failure state resolves musically

## Audio and visual systems

- [x] Single runtime-synthesized stereo stream; no shipped media (FR-020)
- [x] 64-bit 48 kHz sample clock, absolute PPQN conversion, published snapshot
- [x] Fixed voice/event/command storage; callback allocates and blocks on nothing
- [x] Pad, bass, pluck and kick/click/hat voices with master DC block/soft limiting
- [x] Complete pre-generated event schedule with aligned, downbeat-aware stems
- [x] Circular disk, sweep derived from transport, Euclidean petals, glow and trails
- [x] Handcrafted palettes and non-color corruption cues (FR-040–045)
- [x] Phase/recovery-linked progression, damage contraction and final bloom/collapse

## Engineering, tests and release

- [x] Plain modular C99; fixed capacities; no gameplay allocation after initialization
- [x] Headless tests: seed, RNG independence, Euclid, transport, recovery, echo wrap
- [x] Determinism snapshots and 100,000-seed structural fuzz harness
- [x] Developer overlay, stem toggles, phase jump, freeze echo, collision view,
      forced damage/recovery, regeneration, genome print and gallery mode
- [x] Save validation/failure-safe persistence for seed, mode, volume, fullscreen/tutorial
- [x] Exact extracted-size gate with 1,350,000 warning and 1,474,560 hard failure
- [x] CMake debug/release builds and Windows static-link guidance
- [ ] Validate final executable on clean Windows 10 and Windows 11 machines (FR-001/062)
- [ ] Record 30-minute physical audio-device soak and representative GPU/gamepad review
- [ ] Run human balance/legibility review across at least 200 seeds

## Acceptance mapping

FR-005, 010–014, 020–026, 030–045, and 050–053 are represented by automated or
interactive implementation. FR-001–004 and FR-060–062 are build/release properties;
the scripts enforce package structure and size, while clean-machine verification is
necessarily a release QA action. NFR-001–010 are covered structurally and by the test
harness; perceptual and hardware-specific portions remain in the three QA items above.

