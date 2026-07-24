#include "content.h"
#include "flags/flags.h"
#include "generated/content_data.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* The blob is: repeated "##FILE <name>\n" headers followed by file text.
 * We never need the file names at runtime (ids are globally unique and
 * checked by bakery), so the index is only over block headers. */

#define BLOCK_CAP 1024

typedef struct {
    const char *kw;         /* interned keyword pointer into the blob */
    int         kw_len;
    char        id[48];
    const char *body;
    const char *end;
} BlockIndex;

static BlockIndex g_index[BLOCK_CAP];
static int        g_index_n;

static const char *g_blob;
static const char *g_blob_end;

static bool is_block_kw(const char *s, int len)
{
    static const char *kws[] = { "@node", "cut", "board", "scene", 0 };
    for (int i = 0; kws[i]; i++)
        if ((int)strlen(kws[i]) == len && strncmp(kws[i], s, (size_t)len) == 0)
            return true;
    return false;
}

void content_init(void)
{
    g_blob = CONTENT_BLOB;
    g_blob_end = CONTENT_BLOB + sizeof(CONTENT_BLOB) - 1;
    g_index_n = 0;

    const char *p = g_blob;
    while (p < g_blob_end) {
        const char *eol = memchr(p, '\n', (size_t)(g_blob_end - p));
        if (!eol) eol = g_blob_end;

        /* block headers are column-zero; indented lines are block bodies */
        if (p < eol && !isspace((unsigned char)*p) && *p != '#') {
            const char *w = p;
            while (w < eol && !isspace((unsigned char)*w)) w++;
            int kwlen = (int)(w - p);
            if (is_block_kw(p, kwlen)) {
                while (w < eol && isspace((unsigned char)*w)) w++;
                const char *ide = w;
                while (ide < eol && !isspace((unsigned char)*ide)) ide++;
                if (g_index_n > 0) g_index[g_index_n - 1].end = p;
                if (g_index_n < BLOCK_CAP) {
                    BlockIndex *b = &g_index[g_index_n++];
                    b->kw = p;
                    b->kw_len = kwlen;
                    int n = (int)(ide - w);
                    if (n > 47) n = 47;
                    memcpy(b->id, w, (size_t)n);
                    b->id[n] = 0;
                    b->body = (eol < g_blob_end) ? eol + 1 : g_blob_end;
                    b->end = g_blob_end;
                }
            }
        }
        p = (eol < g_blob_end) ? eol + 1 : g_blob_end;
    }
}

static bool kw_is(const BlockIndex *b, const char *keyword)
{
    return (int)strlen(keyword) == b->kw_len &&
           strncmp(b->kw, keyword, (size_t)b->kw_len) == 0;
}

bool content_block(const char *keyword, const char *id, Block *out)
{
    for (int i = 0; i < g_index_n; i++) {
        if (kw_is(&g_index[i], keyword) && strcmp(g_index[i].id, id) == 0) {
            out->p = g_index[i].body;
            out->end = g_index[i].end;
            return true;
        }
    }
    return false;
}

bool content_block_at(const char *keyword, int index, char *id_out, int id_cap,
                      Block *out)
{
    int seen = 0;
    for (int i = 0; i < g_index_n; i++) {
        if (!kw_is(&g_index[i], keyword)) continue;
        if (seen++ != index) continue;
        if (id_out) snprintf(id_out, (size_t)id_cap, "%s", g_index[i].id);
        out->p = g_index[i].body;
        out->end = g_index[i].end;
        return true;
    }
    return false;
}

int content_block_count(const char *keyword)
{
    int n = 0;
    for (int i = 0; i < g_index_n; i++)
        if (kw_is(&g_index[i], keyword)) n++;
    return n;
}

void cur_open(Cursor *c, const Block *b) { c->p = b->p; c->end = b->end; }

bool cur_line(Cursor *c, char *buf, int cap)
{
    while (c->p < c->end) {
        const char *eol = memchr(c->p, '\n', (size_t)(c->end - c->p));
        if (!eol) eol = c->end;
        const char *s = c->p;
        c->p = (eol < c->end) ? eol + 1 : c->end;

        while (s < eol && isspace((unsigned char)*s)) s++;
        const char *e = eol;
        while (e > s && isspace((unsigned char)e[-1])) e--;
        if (s == e) continue;                 /* blank */
        if (*s == '#') continue;              /* comment */
        int n = (int)(e - s);
        if (n > cap - 1) n = cap - 1;
        memcpy(buf, s, (size_t)n);
        buf[n] = 0;
        return true;
    }
    return false;
}

bool word(const char **pp, char *buf, int cap)
{
    const char *p = *pp;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) { *pp = p; return false; }

    int n = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') { if (n < cap - 1) buf[n++] = *p; p++; }
        if (*p == '"') p++;
    } else {
        while (*p && !isspace((unsigned char)*p)) { if (n < cap - 1) buf[n++] = *p; p++; }
    }
    buf[n] = 0;
    *pp = p;
    return true;
}

void rest(const char *p, char *buf, int cap)
{
    while (*p && isspace((unsigned char)*p)) p++;
    int n = (int)strlen(p);
    while (n > 0 && isspace((unsigned char)p[n - 1])) n--;
    if (n >= 2 && p[0] == '"' && p[n - 1] == '"') { p++; n -= 2; }
    if (n > cap - 1) n = cap - 1;
    memcpy(buf, p, (size_t)n);
    buf[n] = 0;
}

bool eq(const char *a, const char *b) { return strcmp(a, b) == 0; }

/* ------------------------------------------------------------- conditions */
static bool cmp_op(int a, const char *op, int b)
{
    if (eq(op, "==")) return a == b;
    if (eq(op, "!=")) return a != b;
    if (eq(op, ">=")) return a >= b;
    if (eq(op, "<=")) return a <= b;
    if (eq(op, ">"))  return a >  b;
    if (eq(op, "<"))  return a <  b;
    return false;
}

static void debracket(char *s)
{
    char *b = strchr(s, ']');
    if (b) *b = 0;
}

/* one name, set or not: "clue.x" reads the clue store, anything else a flag */
static bool truthy(const char *name)
{
    return strncmp(name, "clue.", 5) == 0 ? clue_has(name) : flag_get(name) != 0;
}

bool content_cond(const char *p)
{
    char w[64];
    bool negate = false;
    if (!word(&p, w, sizeof w)) return true;
    if (eq(w, "not")) { negate = true; if (!word(&p, w, sizeof w)) return true; }

    /* `all` is what a chapter gate is made of: a tower floor opens when the
     * whole clue set for that chapter is in the journal (CANON 3.2). */
    if (eq(w, "all") || eq(w, "any")) {
        bool all = eq(w, "all");
        bool acc = all;
        char id[64];
        while (word(&p, id, sizeof id) && id[0]) {
            debracket(id);
            if (!id[0]) break;
            if (all) acc = acc && truthy(id);
            else     acc = acc || truthy(id);
        }
        return negate ? !acc : acc;
    }

    bool r = false;
    if (eq(w, "clue")) {
        char id[64];
        word(&p, id, sizeof id);
        debracket(id);
        r = clue_has(id);
    } else if (eq(w, "flag") || eq(w, "trust")) {
        char key[64], op[8], val[16];
        word(&p, key, sizeof key);
        debracket(key);
        int lhs = eq(w, "trust") ? trust_get(key) : flag_get(key);
        if (!word(&p, op, sizeof op)) return negate ? !(lhs != 0) : (lhs != 0);
        debracket(op);
        if (!op[0]) return negate ? !(lhs != 0) : (lhs != 0);
        word(&p, val, sizeof val);
        debracket(val);
        r = cmp_op(lhs, op, atoi(val));
    }
    return negate ? !r : r;
}

const char *ui_str(const char *key)
{
    /* strings/ui.txt has no block header, so scan the raw blob for "key =" */
    static char out[256];
    size_t klen = strlen(key);
    const char *p = g_blob;
    while (p < g_blob_end) {
        const char *eol = memchr(p, '\n', (size_t)(g_blob_end - p));
        if (!eol) eol = g_blob_end;
        if ((size_t)(eol - p) > klen && strncmp(p, key, klen) == 0) {
            const char *q = p + klen;
            while (q < eol && isspace((unsigned char)*q)) q++;
            if (q < eol && *q == '=') {
                q++;
                while (q < eol && isspace((unsigned char)*q)) q++;
                int n = (int)(eol - q);
                while (n > 0 && isspace((unsigned char)q[n - 1])) n--;
                if (n > (int)sizeof out - 1) n = (int)sizeof out - 1;
                memcpy(out, q, (size_t)n);
                out[n] = 0;
                return out;
            }
        }
        p = (eol < g_blob_end) ? eol + 1 : g_blob_end;
    }
    return key;
}
