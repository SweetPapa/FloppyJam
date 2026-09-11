# AFTERGLOW grounding and implementation contracts

The original requirements and GDD are authoritative. Executable authored tables
in src/story.js are the story bible, character cards, clue table, phrase banks,
word banks and fixed world anchors. src/puzzles.js owns round definitions.
No generated prose can set campaign flags or change the mystery's solution.

Brightwater has Harbor Square, Cannery Row, the Undercroft, Hilltop Gardens and
Observatory, and the Point. Kit delivers flour, a manifest, lamp oil, telescope
parts, and a returned parcel over five nights. Odette's cookbook, Bo's singing,
Pearl's model train, and Idris's star party explain the first four suspicions.
Ada is the Lamplighter, rebuilding Gus's lens. This fact is available only after
Night 5's puzzle. Winnie funds the festival; Ferris narrates; Rosie hosts murals.

Clues, in order: left-handed chalk; boatyard brass; boatyard wrench and tunnel
traffic; 3 a.m. lamp-room testing; Ada's confession. Each fact is awarded only
by finishing the holder's three rounds, and each must be pinned before sleep.
The final board asks for Ada. The finale lights five district lanterns and
returns to the harbor; credits and free exploration follow.

PuzzleHost: createPuzzle(kind, round, seed, lively) returns serializable state;
inputPuzzle(state, action) mutates only puzzle state; hintPuzzle(state) advances
three hints, then exposes autosolvePuzzle; solved is the on-complete signal.
Puzzles own no DOM, clocks, IO or campaign flags. Lively time is passed explicitly.

LLMAdapter: requestDialogue(character, campaign, playerLine, config, memory,
onProgress) returns validated structured dialogue or an authored fallback.
All network access is fetch to the explicitly configured endpoint. Eight-second
total deadline, one regeneration, 160-token ceiling, SSE and JSON support.
Streamed bytes remain quarantined until validation: spoilers and invented
entities must never flash on screen. Approved speech is then chalk-typewritten.
Knowledge gates exclude other characters' secrets and locked clue IDs.
Untrusted player text and memory are user context, never system instructions.
The API key is local settings data and excluded from saves and diagnostic logs.

World: fixed anchor coordinates and rail spokes; seeded filler within district
budgets. New Town changes filler seed only. Pure state is saved in three slots
with versioning, validation and atomic localStorage replacement. Saves include
position, puzzle cursor/board, journal/pins, side activities and finale progress.

Use ES modules; pure simulation modules have Node tests; UI has Playwright tests.
All visual meshes, textures, glyph styling and audio are generated at runtime.
No remote fonts, CDNs, telemetry or asset downloads. Build tools are development
dependencies. Make targets mirror npm commands; CI builds desktop on all three
platforms and runs keyboard campaign playtests on Linux. No size gate.
