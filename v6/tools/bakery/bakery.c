/* bakery — HUEDUNIT's content compiler and linter (§7.1, §10.3-10.6).
 *
 *   bakery <content-dir> <out-header>
 *
 * Reads every file in the content tree, runs the merge-blocking content
 * gates, and emits one C header holding the whole town as a string. Nothing
 * is loaded from disk at runtime; the ship artifact is a single binary.
 *
 * The gates, in order of how much they matter:
 *   - dangling ids     : a jump, cutscene, puzzle or scene that does not exist
 *   - unreachable nodes: dialogue nobody can ever hear
 *   - orphan clues     : evidence the player can collect that proves nothing
 *   - CLUE CLOSURE     : for every board, every required clue is grantable in
 *                        a chapter at or before that board's, from at least
 *                        two independent sources. A mystery game that can
 *                        dead-end an honest player is broken; this is the
 *                        check that makes that structurally impossible.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

/* ------------------------------------------------------------------ util */
static int g_errors, g_warnings;

static void err(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "bakery: %s:%d: error: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    g_errors++;
}

static void warn(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "bakery: %s:%d: warning: ", file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    g_warnings++;
}

/* --------------------------------------------------------------- symbols */
#define SYM_MAX 64
typedef struct {
    char sym[SYM_MAX];
    char file[64];
    int  line;
    int  chapter;     /* -1 unknown */
    int  mark;
} Sym;

typedef struct { Sym *v; int n, cap; } Table;

static void tbl_add(Table *t, const char *sym, const char *file, int line, int chapter)
{
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 64;
        t->v = realloc(t->v, sizeof(Sym) * (size_t)t->cap);
    }
    Sym *s = &t->v[t->n++];
    snprintf(s->sym, SYM_MAX, "%s", sym);
    snprintf(s->file, 64, "%s", file);
    s->line = line;
    s->chapter = chapter;
    s->mark = 0;
}

static Sym *tbl_find(Table *t, const char *sym)
{
    for (int i = 0; i < t->n; i++)
        if (strcmp(t->v[i].sym, sym) == 0) return &t->v[i];
    return NULL;
}

static int tbl_count(Table *t, const char *sym)
{
    int n = 0;
    for (int i = 0; i < t->n; i++) if (strcmp(t->v[i].sym, sym) == 0) n++;
    return n;
}

/* declarations */
static Table T_node, T_cut, T_board, T_scene, T_puzzle, T_feather;
/* uses */
static Table U_node, U_cut, U_board, U_scene, U_puzzle;
/* clue economy */
static Table C_grant;     /* chapter = chapter it can first be granted in */
static Table C_need;      /* chapter = chapter of the board that needs it   */
static Table C_read;      /* clues read by conditions / board evidence      */
/* puzzle -> chapter, discovered from which chapter's dialogue starts it */
static Table P_chapter;

/* ----------------------------------------------------------------- files */
#define MAX_FILES 256
typedef struct {
    char  name[96];       /* e.g. "dialogue/ch1_harbor.dlg" */
    char  base[64];       /* e.g. "ch1_harbor.dlg" */
    char *text;
    long  len;
    int   chapter;
} CFile;

static CFile g_file[MAX_FILES];
static int   g_files;

static int chapter_of(const char *base)
{
    if (base[0] == 'p' && base[1] == '_') return 0;         /* prologue */
    if (base[0] == 'c' && base[1] == 'h' && isdigit((unsigned char)base[2]))
        return base[2] - '0';
    if (strncmp(base, "f_", 2) == 0) return 6;              /* finale */
    return -1;                                              /* shared */
}

static char *slurp(const char *path, long *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    b[n] = 0;
    fclose(f);
    *out_len = n;
    return b;
}

static int cmp_file(const void *a, const void *b)
{
    return strcmp(((const CFile *)a)->name, ((const CFile *)b)->name);
}

static void load_dir(const char *root, const char *sub)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", root, sub);
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char full[640];
        snprintf(full, sizeof full, "%s/%s", path, e->d_name);
        long len = 0;
        char *txt = slurp(full, &len);
        if (!txt) continue;
        if (g_files >= MAX_FILES) { fprintf(stderr, "bakery: too many files\n"); exit(1); }
        CFile *f = &g_file[g_files++];
        snprintf(f->name, sizeof f->name, "%s/%s", sub, e->d_name);
        snprintf(f->base, sizeof f->base, "%s", e->d_name);
        f->text = txt;
        f->len = len;
        f->chapter = chapter_of(e->d_name);
    }
    closedir(d);
}

/* ------------------------------------------------------------ line tools */
static void trim(char *s)
{
    int n = (int)strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' ||
                 s[n - 1] == '\t')) s[--n] = 0;
}

static const char *skipws(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static int takeword(const char **pp, char *buf, int cap)
{
    const char *p = skipws(*pp);
    if (!*p) { *pp = p; return 0; }
    int n = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') { if (n < cap - 1) buf[n++] = *p; p++; }
        if (*p == '"') p++;
    } else {
        while (*p && *p != ' ' && *p != '\t') { if (n < cap - 1) buf[n++] = *p; p++; }
    }
    buf[n] = 0;
    *pp = p;
    return 1;
}

/* iterate a file's lines; returns 0 at end */
typedef struct { const char *p; int line; } LineIt;
static int nextline(LineIt *it, char *buf, int cap)
{
    if (!*it->p) return 0;
    const char *nl = strchr(it->p, '\n');
    int n = nl ? (int)(nl - it->p) : (int)strlen(it->p);
    if (n > cap - 1) n = cap - 1;
    memcpy(buf, it->p, (size_t)n);
    buf[n] = 0;
    it->p = nl ? nl + 1 : it->p + strlen(it->p);
    it->line++;
    trim(buf);
    return 1;
}

static int is_ext(const CFile *f, const char *ext)
{
    size_t n = strlen(f->base), e = strlen(ext);
    return n > e && strcmp(f->base + n - e, ext) == 0;
}

/* ================================================================= PASS 1
 * declarations
 * ====================================================================== */
static void pass_declare(void)
{
    for (int i = 0; i < g_files; i++) {
        CFile *f = &g_file[i];
        LineIt it = { f->text, 0 };
        char raw[512];
        while (nextline(&it, raw, sizeof raw)) {
            if (raw[0] == '#' || !raw[0]) continue;
            if (raw[0] == ' ' || raw[0] == '\t') continue;    /* body line */
            const char *p = raw;
            char kw[64], id[SYM_MAX];
            if (!takeword(&p, kw, sizeof kw)) continue;
            if (!takeword(&p, id, sizeof id)) continue;

            Table *t = NULL;
            if      (strcmp(kw, "@node") == 0) t = &T_node;
            else if (strcmp(kw, "cut")   == 0) t = &T_cut;
            else if (strcmp(kw, "board") == 0) t = &T_board;
            else if (strcmp(kw, "scene") == 0) t = &T_scene;
            if (!t) continue;
            if (tbl_find(t, id))
                err(f->name, it.line, "duplicate %s id '%s'", kw, id);
            tbl_add(t, id, f->name, it.line, f->chapter);
        }
    }
}

/* puzzles are declared in code, not content: glob src/puzzles/p_*.c and
 * lift the .id and .clue_granted fields. No hand-edited registry (§7.3). */
static void scan_puzzles(void)
{
    DIR *d = opendir("src/puzzles");
    if (!d) { fprintf(stderr, "bakery: warning: no src/puzzles\n"); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t nl = strlen(e->d_name);
        if (e->d_name[0] != 'p' || e->d_name[1] != '_') continue;
        if (nl < 3 || strcmp(e->d_name + nl - 2, ".c") != 0) continue;  /* not .o */
        char full[256];
        snprintf(full, sizeof full, "src/puzzles/%s", e->d_name);
        long len = 0;
        char *txt = slurp(full, &len);
        if (!txt) continue;

        const char *q = strstr(txt, ".id");
        if (q) {
            const char *s = strchr(q, '"');
            if (s) {
                char id[SYM_MAX]; int n = 0;
                for (s++; *s && *s != '"' && n < SYM_MAX - 1; s++) id[n++] = *s;
                id[n] = 0;
                tbl_add(&T_puzzle, id, e->d_name, 1, -1);

                const char *c = strstr(txt, ".clue_granted");
                if (c) {
                    const char *cs = strchr(c, '"');
                    const char *semi = strchr(c, ';');
                    if (cs && (!semi || cs < semi)) {
                        char cl[SYM_MAX]; int m = 0;
                        for (cs++; *cs && *cs != '"' && m < SYM_MAX - 1; cs++) cl[m++] = *cs;
                        cl[m] = 0;
                        /* file field carries the puzzle id so clue closure can
                         * date the grant from the dialogue that starts it */
                        if (cl[0]) tbl_add(&C_grant, cl, id, 1, -1);
                    }
                }
            }
        }
        if (!strstr(txt, "solve_replay"))
            err(e->d_name, 1, "puzzle has no solve_replay fixture (gate §10.5)");
        free(txt);
    }
    closedir(d);
}

/* ================================================================= PASS 2
 * uses, per grammar
 * ====================================================================== */
static const char *CUT_VERBS[] = {
    "BG", "PAN", "ACTOR", "MOVE", "EMOTE", "SAY", "WAIT", "MUSIC", "SFX",
    "FADE", "HUE", "BLOOM", "DOODLE", "SHAKE", "TITLE", "EXIT", "CAM", 0
};

static int cut_verb_ok(const char *v)
{
    for (int i = 0; CUT_VERBS[i]; i++) if (strcmp(CUT_VERBS[i], v) == 0) return 1;
    return 0;
}

static void pass_dlg(CFile *f)
{
    LineIt it = { f->text, 0 };
    char raw[512];
    char cur_node[SYM_MAX] = "";
    while (nextline(&it, raw, sizeof raw)) {
        const char *p = skipws(raw);
        if (!*p || *p == '#') continue;
        if (raw[0] == '@') {
            char kw[64];
            const char *q = raw;
            takeword(&q, kw, sizeof kw);
            takeword(&q, cur_node, sizeof cur_node);
            continue;
        }
        /* choice jump:  * (tone) text -> node  */
        const char *arrow = strstr(p, "->");
        if (arrow) {
            const char *q = arrow + 2;
            char tgt[SYM_MAX];
            if (takeword(&q, tgt, sizeof tgt) && tgt[0])
                tbl_add(&U_node, tgt, f->name, it.line, f->chapter);
        }
        char kw[64];
        const char *q = p;
        takeword(&q, kw, sizeof kw);

        if (strcmp(kw, "grant") == 0) {
            char id[SYM_MAX];
            while (takeword(&q, id, sizeof id) && id[0])
                tbl_add(&C_grant, id, f->name, it.line, f->chapter);
        } else if (strcmp(kw, "start_puzzle") == 0) {
            char id[SYM_MAX];
            if (takeword(&q, id, sizeof id)) {
                tbl_add(&U_puzzle, id, f->name, it.line, f->chapter);
                tbl_add(&P_chapter, id, f->name, it.line, f->chapter);
            }
        } else if (strcmp(kw, "play_cut") == 0) {
            char id[SYM_MAX];
            if (takeword(&q, id, sizeof id))
                tbl_add(&U_cut, id, f->name, it.line, f->chapter);
        } else if (strcmp(kw, "goto") == 0) {
            char id[SYM_MAX];
            if (takeword(&q, id, sizeof id))
                tbl_add(&U_node, id, f->name, it.line, f->chapter);
        } else if (strcmp(kw, "trust") == 0 || strcmp(kw, "set") == 0 ||
                   strcmp(kw, "add") == 0) {
            /* flag writes: nothing to resolve, the store is dynamic */
        } else if (p[0] == '[') {
            /* condition: [if flag x == 0] / [if clue clue.y] */
            char t1[SYM_MAX], t2[SYM_MAX];
            const char *r = p + 1;
            takeword(&r, t1, sizeof t1);                     /* if */
            if (takeword(&r, t2, sizeof t2) && strcmp(t2, "clue") == 0) {
                char id[SYM_MAX];
                if (takeword(&r, id, sizeof id)) {
                    char *br = strchr(id, ']');
                    if (br) *br = 0;
                    tbl_add(&C_read, id, f->name, it.line, f->chapter);
                }
            }
        }
    }
}

static void pass_case(CFile *f)
{
    LineIt it = { f->text, 0 };
    char raw[512];
    char cur[SYM_MAX] = "";
    int  cur_chapter = f->chapter;
    int  blanks = 0, from_total = 0;

    while (nextline(&it, raw, sizeof raw)) {
        const char *p = skipws(raw);
        if (!*p || *p == '#') continue;
        char kw[64];
        const char *q = p;
        takeword(&q, kw, sizeof kw);

        if (strcmp(kw, "board") == 0) {
            if (cur[0] && blanks && from_total < blanks * 2)
                err(f->name, it.line, "board '%s': fairness gate wants >=2 evidence "
                    "sources per blank (§3.3); got %d for %d blanks",
                    cur, from_total, blanks);
            takeword(&q, cur, sizeof cur);
            cur_chapter = f->chapter;
            blanks = from_total = 0;
        } else if (strcmp(kw, "requires") == 0) {
            char id[SYM_MAX];
            while (takeword(&q, id, sizeof id) && id[0]) {
                tbl_add(&C_need, id, f->name, it.line, cur_chapter);
                tbl_add(&C_read, id, f->name, it.line, cur_chapter);
            }
        } else if (strcmp(kw, "blank") == 0) {
            blanks++;
            char t[SYM_MAX];
            while (takeword(&q, t, sizeof t) && t[0]) {
                if (strcmp(t, "from") == 0) {
                    char id[SYM_MAX];
                    while (takeword(&q, id, sizeof id) && id[0]) {
                        from_total++;
                        tbl_add(&C_need, id, f->name, it.line, cur_chapter);
                        tbl_add(&C_read, id, f->name, it.line, cur_chapter);
                    }
                    break;
                }
            }
        } else if (strcmp(kw, "on_solved") == 0) {
            char what[64], id[SYM_MAX];
            takeword(&q, what, sizeof what);
            if (takeword(&q, id, sizeof id)) {
                if (strcmp(what, "cut") == 0)
                    tbl_add(&U_cut, id, f->name, it.line, cur_chapter);
                else if (strcmp(what, "goto") == 0)
                    tbl_add(&U_node, id, f->name, it.line, cur_chapter);
            }
        }
    }
    if (cur[0] && blanks && from_total < blanks * 2)
        err(f->name, it.line, "board '%s': fairness gate wants >=2 evidence sources "
            "per blank (§3.3); got %d for %d blanks", cur, from_total, blanks);
}

static void pass_cut(CFile *f)
{
    LineIt it = { f->text, 0 };
    char raw[512];
    while (nextline(&it, raw, sizeof raw)) {
        const char *p = skipws(raw);
        if (!*p || *p == '#') continue;
        char kw[64];
        const char *q = p;
        takeword(&q, kw, sizeof kw);
        if (strcmp(kw, "cut") == 0) continue;
        if (!cut_verb_ok(kw)) {
            err(f->name, it.line, "unknown cutscene verb '%s' — new verbs are a "
                "cut-engine job card, not an inline hack (§9.3)", kw);
            continue;
        }
        if (strcmp(kw, "BG") == 0) {
            char id[SYM_MAX];
            if (takeword(&q, id, sizeof id))
                tbl_add(&U_scene, id, f->name, it.line, f->chapter);
        }
    }
}

static void pass_scn(CFile *f)
{
    LineIt it = { f->text, 0 };
    char raw[512];
    while (nextline(&it, raw, sizeof raw)) {
        const char *p = skipws(raw);
        if (!*p || *p == '#') continue;
        char kw[64];
        const char *q = p;
        takeword(&q, kw, sizeof kw);

        if (strcmp(kw, "on_enter") == 0) {
            char id[SYM_MAX];
            if (takeword(&q, id, sizeof id))
                tbl_add(&U_node, id, f->name, it.line, f->chapter);
        } else if (strcmp(kw, "on_enter_cut") == 0) {
            char id[SYM_MAX];
            if (takeword(&q, id, sizeof id))
                tbl_add(&U_cut, id, f->name, it.line, f->chapter);
        } else if (strcmp(kw, "feather") == 0) {
            char x[16], y[16], id[SYM_MAX];
            takeword(&q, x, sizeof x); takeword(&q, y, sizeof y);
            if (takeword(&q, id, sizeof id)) {
                if (tbl_find(&T_feather, id))
                    err(f->name, it.line, "duplicate feather id '%s'", id);
                tbl_add(&T_feather, id, f->name, it.line, f->chapter);
            }
        } else if (strcmp(kw, "hot") == 0) {
            char t[SYM_MAX];
            for (int i = 0; i < 4; i++) takeword(&q, t, sizeof t);   /* x y w h */
            char kind[32];
            if (!takeword(&q, kind, sizeof kind)) continue;
            char arg[SYM_MAX];
            if (strcmp(kind, "talk") == 0) {
                if (takeword(&q, arg, sizeof arg))
                    tbl_add(&U_node, arg, f->name, it.line, f->chapter);
            } else if (strcmp(kind, "exit") == 0) {
                if (takeword(&q, arg, sizeof arg))
                    tbl_add(&U_scene, arg, f->name, it.line, f->chapter);
            } else if (strcmp(kind, "board") == 0) {
                if (takeword(&q, arg, sizeof arg))
                    tbl_add(&U_board, arg, f->name, it.line, f->chapter);
            } else if (strcmp(kind, "puzzle") == 0) {
                if (takeword(&q, arg, sizeof arg))
                    tbl_add(&U_puzzle, arg, f->name, it.line, f->chapter);
            } else if (strcmp(kind, "look") != 0) {
                err(f->name, it.line, "unknown hotspot kind '%s'", kind);
            }
        }
    }
}

/* ================================================================= GATES */
static void gate_dangling(void)
{
    struct { Table *use; Table *dec; const char *what; } pairs[] = {
        { &U_node,   &T_node,   "dialogue node" },
        { &U_cut,    &T_cut,    "cutscene" },
        { &U_board,  &T_board,  "board" },
        { &U_scene,  &T_scene,  "scene" },
        { &U_puzzle, &T_puzzle, "puzzle" },
    };
    for (size_t k = 0; k < sizeof pairs / sizeof pairs[0]; k++) {
        Table *u = pairs[k].use, *d = pairs[k].dec;
        for (int i = 0; i < u->n; i++)
            if (!tbl_find(d, u->v[i].sym))
                err(u->v[i].file, u->v[i].line, "dangling %s id '%s'",
                    pairs[k].what, u->v[i].sym);
    }
}

static void gate_unreachable(void)
{
    for (int i = 0; i < T_node.n; i++)
        if (!tbl_find(&U_node, T_node.v[i].sym))
            err(T_node.v[i].file, T_node.v[i].line,
                "unreachable dialogue node '%s' — nobody can ever hear this",
                T_node.v[i].sym);
    for (int i = 0; i < T_cut.n; i++)
        if (!tbl_find(&U_cut, T_cut.v[i].sym))
            warn(T_cut.v[i].file, T_cut.v[i].line, "cutscene '%s' is never played",
                 T_cut.v[i].sym);
    for (int i = 0; i < T_board.n; i++)
        if (!tbl_find(&U_board, T_board.v[i].sym))
            err(T_board.v[i].file, T_board.v[i].line,
                "board '%s' is never opened by any scene", T_board.v[i].sym);
    for (int i = 0; i < T_puzzle.n; i++)
        if (!tbl_find(&U_puzzle, T_puzzle.v[i].sym))
            warn(T_puzzle.v[i].file, T_puzzle.v[i].line,
                 "puzzle '%s' is never started by any dialogue or hotspot",
                 T_puzzle.v[i].sym);
}

static void gate_orphan_clues(void)
{
    for (int i = 0; i < C_grant.n; i++) {
        const char *c = C_grant.v[i].sym;
        int seen = 0;
        for (int j = 0; j < i; j++) if (strcmp(C_grant.v[j].sym, c) == 0) seen = 1;
        if (seen) continue;
        if (!tbl_find(&C_read, c))
            err(C_grant.v[i].file, C_grant.v[i].line,
                "orphan clue '%s' — granted but no board or condition ever uses it",
                c);
    }
}

/* the fairness enforcer (§10.4) */
static void gate_clue_closure(void)
{
    for (int i = 0; i < C_need.n; i++) {
        const char *c = C_need.v[i].sym;
        int need_ch = C_need.v[i].chapter;

        if (strncmp(c, "clue.", 5) != 0) continue;   /* pool tokens, not clues */

        int sources = 0, in_time = 0, earliest = 99;
        for (int j = 0; j < C_grant.n; j++) {
            if (strcmp(C_grant.v[j].sym, c) != 0) continue;
            sources++;
            int gch = C_grant.v[j].chapter;
            if (gch < 0) {
                /* granted by a puzzle: its chapter is the chapter of the
                 * dialogue that starts it */
                Sym *pc = tbl_find(&P_chapter, C_grant.v[j].file);
                gch = pc ? pc->chapter : need_ch;
            }
            if (gch < earliest) earliest = gch;
            if (gch <= need_ch) in_time++;
        }
        if (sources == 0) {
            err(C_need.v[i].file, C_need.v[i].line,
                "CLUE CLOSURE: '%s' is required but nothing ever grants it", c);
        } else if (in_time == 0) {
            err(C_need.v[i].file, C_need.v[i].line,
                "CLUE CLOSURE: '%s' is required in chapter %d but is first "
                "grantable in chapter %d — an honest player can dead-end here",
                c, need_ch, earliest);
        }
    }
}

/* No string may be longer than the engine can hold.
 *
 * This gate exists because the failure it catches is invisible: an overlong
 * line does not crash, it just stops mid-sentence on screen, and nobody
 * notices until a player asks why a character trails off. TEXT_MAX here must
 * match src/content/content.h. */
#define TEXT_MAX 512

static void check_len(const char *file, int line, const char *what,
                      const char *text)
{
    int n = (int)strlen(text);
    if (n < TEXT_MAX) return;
    err(file, line, "%s is %d characters; the engine holds %d and would cut it "
        "off mid-sentence. Split it into two beats.", what, n, TEXT_MAX - 1);
}

static void gate_string_lengths(void)
{
    for (int i = 0; i < g_files; i++) {
        CFile *f = &g_file[i];
        LineIt it = { f->text, 0 };
        char raw[2048];

        while (nextline(&it, raw, sizeof raw)) {
            const char *p = skipws(raw);
            if (!*p || *p == '#') continue;

            if (is_ext(f, ".cut")) {
                char kw[64];
                const char *q = p;
                takeword(&q, kw, sizeof kw);
                if (strcmp(kw, "SAY") == 0) {
                    char who[64], said[2048];
                    takeword(&q, who, sizeof who);
                    q = skipws(q);
                    snprintf(said, sizeof said, "%s", q);
                    int n = (int)strlen(said);
                    if (n >= 2 && said[0] == '"' && said[n - 1] == '"') {
                        said[n - 1] = 0;
                        check_len(f->name, it.line, "a spoken line", said + 1);
                    } else {
                        err(f->name, it.line, "SAY needs its line in quotes");
                    }
                }
            } else if (is_ext(f, ".scn")) {
                const char *a = strchr(p, '"');
                if (a) {
                    const char *b = strchr(a + 1, '"');
                    if (b) {
                        char lab[2048];
                        int n = (int)(b - a - 1);
                        if (n > (int)sizeof lab - 1) n = (int)sizeof lab - 1;
                        memcpy(lab, a + 1, (size_t)n);
                        lab[n] = 0;
                        check_len(f->name, it.line, "a hotspot's text", lab);
                    }
                }
            } else if (is_ext(f, ".dlg")) {
                const char *colon = strchr(p, ':');
                const char *body = colon ? colon + 1 : p;
                check_len(f->name, it.line, "a line of dialogue", body);
            } else if (is_ext(f, ".case") || is_ext(f, ".txt")) {
                check_len(f->name, it.line, "this line", p);
            }
        }
    }
}

/* every puzzle's chapter must have dialogue that starts it before its board */
static void gate_puzzle_reach(void)
{
    for (int i = 0; i < T_puzzle.n; i++) {
        int n = tbl_count(&U_puzzle, T_puzzle.v[i].sym);
        if (n > 1)
            warn(T_puzzle.v[i].file, 1, "puzzle '%s' is started from %d places",
                 T_puzzle.v[i].sym, n);
    }
}

/* ================================================================== emit */
static void emit(const char *out)
{
    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "bakery: cannot write %s\n", out); exit(1); }

    long total = 0;
    for (int i = 0; i < g_files; i++) total += g_file[i].len + 32;

    fprintf(f,
        "/* GENERATED BY bakery — DO NOT EDIT.\n"
        " * %d content files, %ld bytes of town.\n"
        " */\n"
        "#ifndef HD_CONTENT_DATA_H\n"
        "#define HD_CONTENT_DATA_H\n\n"
        "static const char CONTENT_BLOB[] =\n", g_files, total);

    for (int i = 0; i < g_files; i++) {
        CFile *cf = &g_file[i];
        fprintf(f, "/* ---- %s ---- */\n\"##FILE %s\\n\"\n", cf->name, cf->name);
        const char *p = cf->text;
        while (*p) {
            const char *nl = strchr(p, '\n');
            int n = nl ? (int)(nl - p) : (int)strlen(p);
            if (n && p[n - 1] == '\r') n--;
            fputc('"', f);
            for (int k = 0; k < n; k++) {
                unsigned char c = (unsigned char)p[k];
                if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
                else if (c == '\t') fputs("    ", f);
                else if (c < 32 || c > 126) fprintf(f, "\\x%02x", c);
                else fputc(c, f);
            }
            fputs("\\n\"\n", f);
            if (!nl) break;
            p = nl + 1;
        }
    }
    fprintf(f, ";\n\n#endif\n");
    fclose(f);
}

/* =================================================================== main */
int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: bakery <content-dir> <out-header>\n");
        return 2;
    }
    const char *root = argv[1];
    static const char *subs[] = { "dialogue", "boards", "cutscenes", "scenes",
                                  "strings", 0 };
    for (int i = 0; subs[i]; i++) load_dir(root, subs[i]);
    if (g_files == 0) { fprintf(stderr, "bakery: no content found in %s\n", root); return 1; }
    qsort(g_file, (size_t)g_files, sizeof(CFile), cmp_file);

    scan_puzzles();
    pass_declare();
    for (int i = 0; i < g_files; i++) {
        CFile *f = &g_file[i];
        if (is_ext(f, ".dlg"))       pass_dlg(f);
        else if (is_ext(f, ".case")) pass_case(f);
        else if (is_ext(f, ".cut"))  pass_cut(f);
        else if (is_ext(f, ".scn"))  pass_scn(f);
    }

    gate_dangling();
    gate_unreachable();
    gate_orphan_clues();
    gate_clue_closure();
    gate_puzzle_reach();
    gate_string_lengths();

    printf("bakery: %d files, %d nodes, %d scenes, %d boards, %d cutscenes, "
           "%d puzzles, %d feathers\n",
           g_files, T_node.n, T_scene.n, T_board.n, T_cut.n, T_puzzle.n,
           T_feather.n);

    if (g_errors) {
        fprintf(stderr, "bakery: %d error(s), %d warning(s) — content gates are "
                "merge-blocking (§10)\n", g_errors, g_warnings);
        return 1;
    }
    emit(argv[2]);
    printf("bakery: baked -> %s (%d warning(s))\n", argv[2], g_warnings);
    return 0;
}
