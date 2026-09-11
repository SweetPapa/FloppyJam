# Investigation expansion review

The original game has a strong premise and character writing. Its weak point was agency between conversations: most scenery was flavor text, useful evidence often arrived at the end of a mini-game, and progression meant walking back through the same hub. A total rewrite or combat system would work against what makes this town distinctive.

This update gives the player three complementary activities: follow leads in the main mystery, make deductions from the evidence they have collected, and investigate small stories through objects in the town. The optional mysteries bring rewards back into the square, so exploration leaves a visible trace. The map removes repeated walking as a cost of curiosity. Neither optional completion nor friendship gates the main story.

## Blockers and friction found during review

- The board interpreter held four answer pools, while chapters two, four and five authored five or six. Some required tokens were never loaded. The festival board also exceeded the ten-line limit, removing the line containing its last blank. Runtime board validation and actual interpreted solves now cover these cases.
- The HUD used restored hue count as its chapter index. Solving the prologue unlocks the harbor without restoring a hue, so its objective could remain on the prologue. One investigation model now derives progression from solved boards and reads required clues from the authored board.
- The teaching lock generated local pairwise comparisons that did not guarantee a unique ordering, while its solution checker required one exact hidden permutation. Its four comparisons now establish a complete order of all five distinct heights. The fixture checks all legal permutations.
- Hint notes could remain over puzzle controls while input reached the puzzle underneath. Hints now pause interaction, can be closed and reopened, and do not charge twice to reread a purchased tier.
- Several puzzles never counted failed attempts, so a skip unlocked only by failures could remain inaccessible. Optional cooperation is now available through an explicit confirmation note.
- The journal drew only ten completed puzzles but tested clicks against a longer invisible list. Drawing and hit testing now use the same scrolling range.
- Returning to a solved board could replay its reward or restore an earlier palette stage. Solved boards now open for reading. Drafts and hint tiers use durable flags.
- Direct save overwrites and early flag-store resets made interrupted files risky. New saves use a temporary file and rename, and loading validates before committing. Existing saves remain readable; screenshots and tests cannot overwrite them.

## Scope and limits

The fifteen existing mini-games and original main storyline remain. Six two-observation mysteries are new; their rewards are optional, not a second progression gate. New objects use the same scene walking and inspection system as other interactions. There are no added third-party raster assets or dependencies beyond the embedded licensed font and Python build helper.

The campaign test executes real conversation branches and gate conditions, but solves mini-games using their fixtures and supplies board answers from their definitions. It verifies a reachable, completable story; it is not evidence that every puzzle is well paced for a first-time human player. The native test separately exercises actual input routing and drawing. A longer fresh-player session is still useful for judging clue ambiguity, conversation length, and whether the optional mysteries feel too obvious.

No Windows or Linux play session was performed for this update. Existing user save files were left in place; validation and captures use isolated or read-only sessions.
