/* content — the baked text blob and the line cursor every interpreter shares.
 *
 * `bakery` concatenates the content tree into src/generated/content_data.h as one
 * C string. Nothing is read from disk at runtime; the shipped artifact is a
 * single binary with the whole town inside it.
 *
 * Interpreters do not own parsers for tokens — they own parsers for *verbs*.
 * The line/word plumbing lives here so .dlg, .cut, .case and .scn all read
 * the same way.
 */
#ifndef HD_CONTENT_H
#define HD_CONTENT_H

#include <stdbool.h>

void content_init(void);

/* --- blocks ---------------------------------------------------------------
 * Every content file is a sequence of blocks introduced by a keyword line:
 *   @node <id>      (.dlg)      cut <id>    (.cut)
 *   board <id>      (.case)     scene <id>  (.scn)
 * content_block() finds one by keyword+id anywhere in the blob.
 */
typedef struct {
    const char *p;      /* first byte after the header line */
    const char *end;    /* one past the last byte of the block */
} Block;

bool content_block(const char *keyword, const char *id, Block *out);
/* enumerate: index-th block with this keyword; id copied out (may be NULL) */
bool content_block_at(const char *keyword, int index, char *id_out, int id_cap,
                      Block *out);
int  content_block_count(const char *keyword);

/* --- line cursor ---------------------------------------------------------- */
typedef struct {
    const char *p, *end;
} Cursor;

void cur_open(Cursor *c, const Block *b);
/* Next non-blank, non-comment line, indentation stripped. Copies into buf.
 * Returns false at end of block. */
bool cur_line(Cursor *c, char *buf, int cap);

/* --- word helpers on a line ----------------------------------------------- */
/* Reads a whitespace-delimited word (or a "quoted string", quotes stripped)
 * from *pp, advancing it. Returns false when the line is exhausted. */
bool word(const char **pp, char *buf, int cap);
/* Rest of the line, trimmed; quotes stripped if the whole rest is quoted. */
void rest(const char *p, char *buf, int cap);
/* strcmp == 0, spelled for readability at call sites */
bool eq(const char *a, const char *b);

/* --- conditions ----------------------------------------------------------
 * One evaluator, shared by dialogue, scenes and cutscenes, so a condition
 * means exactly the same thing everywhere it is written:
 *     flag <name> [<op> <n>]   clue <id>   trust <npc> <op> <n>
 * optionally prefixed with `not`. `p` points just past the `if`.
 */
bool content_cond(const char *p);

/* --- strings/ui.txt -------------------------------------------------------
 * key = value lines. Missing keys return the key itself, which makes a
 * missing string loud on screen rather than silent. */
const char *ui_str(const char *key);

#endif
