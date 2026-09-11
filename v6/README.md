# HUEDUNIT — v6

A cozy detective adventure about a seaside town that has lost its colors, a missing Tinter, and a magpie everyone has blamed too quickly. Help the people of Prismbrook, gather evidence, and work out what happened inside the locked Prismworks.

The game has fourteen locations, fifteen mini-games, six main deductions, a finale, and six optional neighborhood mysteries. There is no combat or time pressure.

## Play

```sh
cd v6                 # from the repository root
make run
```

Build dependencies: a C11 compiler, Python 3, raylib and pkg-config. On macOS, the existing Homebrew raylib installation is used. Art, dialogue, music and the licensed Barlow font are embedded in the executable; runtime does not need an assets folder. The macOS binary links to raylib dynamically.

| Action | Input |
| --- | --- |
| Walk, talk, inspect an object | Left click |
| Hurry to an interaction | Double-click |
| Reveal interaction labels | Hold Space while exploring |
| Advance a conversation or inspection | Click, Space or Enter |
| Choose a dialogue response | Click or 1–3 |
| Casebook: leads, town map, little mysteries | M or the Casebook button |
| Journal: people, collected evidence, solved boards, replays | J or Tab |
| Settings / leave a puzzle or board / dismiss a hint | Escape |
| Skip a cutscene | Escape |
| Screenshot | F12 |

Map travel opens with the story, using the same district gates as walking. The first puzzle hint is free, further tiers cost one feather, and previously purchased hints can be read again. **Work together** offers an optional way to finish a puzzle and receive its evidence; a confirmation note gives you a chance to return to the puzzle instead.

## The investigation expansion

- **A casebook with useful leads.** The current question and evidence count follow solved deductions, independently of color animations. Leads point to people and places with missing evidence. Once ready, open the deduction directly from your notes.
- **A town map.** Travel between unlocked locations, see where you are, and keep the walking routes for exploring.
- **Six little mysteries.** Find two physical observations in each neighborhood, connect them, and choose an explanation. These are optional and have no wrong-answer penalty. Rewards include two feathers, friendship, and a keepsake displayed in the square. Repeated clicks cannot award the same reward again.
- **Clearer deduction boards.** Select a blank to see its answer category and supporting observations. Draft answers and hint tiers persist. Solved cases can be read from the journal without replaying their rewards or color changes.
- **More readable presentation.** Embedded scalable type, warm paper controls, quieter interaction glints, distinct props in secondary locations, labeled exits, and characters who approach people and observations from the side.
- **Fairer puzzles and fewer interruptions.** Petra's teaching lock now gives a complete ordering with exactly one solution. Hints have dismissible notes that stop input from reaching puzzle pieces underneath. Every completed puzzle is reachable in the journal's scrolling replay list.

The main story and its warm resolution remain intact. This expansion adds things to investigate between conversations rather than adding combat, grinding, or time gates.

## Saves

The game stores `huedunit0.sav` and `huedunit.cfg` in its working directory. Run it from the same directory to continue an existing case. Version 1 saves are supported. New saves use atomic replacement, preserve evidence discovery order and unknown flags, and validate the file before replacing the live case. Version 2 detects missing save footers. The format is not intended to recover every possible form of file corruption.

Board drafts, purchased hint tiers, observations and mystery rewards persist. Mini-game piece positions restart when you re-enter a puzzle. Puzzle replays do not award story evidence or change completion flags, although requesting a new hint still spends feathers.

Screenshot and native test modes disable all save and settings writes. Test binaries run in disposable directories.

## Build and verify

```sh
make                 # optimized release build
make debug           # clean rebuild with debug information
make check           # content lint, headless tests, release build, size report
make size            # informational only; no floppy-size ceiling
make native-smoke    # build the test that opens a real raylib window
make clean
```

For the native input and layout checks:

```sh
mkdir -p artifacts/review
cd artifacts/review
../../huedunit_smoke
```

The native harness replaces input polling in its own test executable; it runs the production app and renderer without sending mouse or keyboard events to other applications. It writes screenshots in its working directory and never writes player saves.

Verification includes:

- Flag-store and content tests, including clue sources and valid scene/dialogue targets.
- All fifteen puzzle solve fixtures at three difficulties and six seeds: 270 runs. Fixtures demonstrate solvability, not human difficulty or enjoyment.
- Campaign traversal through the real dialogue, condition and board interpreters using three dialogue-choice patterns; all six deductions and the finale must complete. This catches runtime truncation that source-only linting missed.
- Optional mystery gating, wrong answers, idempotent rewards, old-save loading, interrupted saves, settings bounds and read-only captures.
- Native mouse/keyboard interaction checks for observations, casebook travel, mystery answers, hints, cooperation, the final journal replay, and board layouts at all three text sizes.

The verified platform for this update is macOS with raylib 6.0. Windows cross-compilation remains available with `make windows RAYLIB_WIN=/path/to/mingw-raylib`, but this update has not been played on Windows or Linux.

## Captures

```sh
./huedunit --shot title title.png
./huedunit --shot scene:ch1_dock dock.png
./huedunit --shot map map.png
./huedunit --shot mysteries mysteries.png
./huedunit --shot board:ch5_where board.png
./huedunit --shot puzzle:water_flow+hint hint.png
./huedunit --shot dialogue:p_mayor_gate conversation.png
./huedunit --shot journal journal.png
```

Screenshots use fabricated review state. They are not saved-game screenshots. The final optional argument is a positive number of frames to wait before capture.

The original story and design reference is `docs/CANON.md`; engine contracts are in `docs/API.md`. The new casework model, map, observation content and UI live in `src/investigation/`. Optional cases can be expanded there without modifying the main mystery's clue economy. See `docs/EXPANSION_REVIEW.md` for the rationale and testing limits.

Barlow Medium is distributed under the SIL Open Font License in `assets/fonts/OFL.txt`.
