/* §10.5 gate: every registered puzzle ships a solve_replay fixture, and the
 * host runs all of them headless, at every difficulty, across a spread of
 * seeds. No window, no audio device, no mouse.
 *
 * §10.6 gate: seeded puzzles are deterministic — the same seed must produce
 * the same instance, or re-entering a puzzle would show a different one.
 */
#include "puzzle/puzzle.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void check(bool ok, const char *what, const char *detail)
{
    if (ok) return;
    printf("  FAIL  %s: %s\n", what, detail);
    g_fail++;
}

int main(void)
{
    int n = puzzle_count();
    printf("test_puzzles: %d registered\n", n);
    if (n != 15) {
        printf("  FAIL  expected 15 mini-games (CANON 5.1), found %d\n", n);
        g_fail++;
    }

    static const unsigned SEEDS[6] = { 1u, 7u, 0x9e3779b9u, 12345u, 0xdeadbeefu, 99u };

    for (int i = 0; i < n; i++) {
        const pz_def *d = puzzle_at(i);
        check(d->id && d->id[0], "definition", "puzzle has no id");
        check(d->title && d->title[0], d->id, "puzzle has no title");
        check(d->init != NULL, d->id, "no init");
        check(d->update != NULL, d->id, "no update");
        check(d->draw != NULL, d->id, "no draw");
        check(d->hint != NULL, d->id, "no hint");
        check(d->solve_replay != NULL, d->id, "no solve_replay fixture");
        if (!d->solve_replay || !d->init || !d->hint) continue;

        int runs = 0;
        for (int diff = 0; diff <= 2; diff++)
            for (int s = 0; s < 6; s++) {
                pz_ctx ctx = { 0 };
                ctx.seed = SEEDS[s];
                ctx.difficulty = diff;
                ctx.area = (Rectangle){ 170, 130, 940, 430 };
                bool ok = d->solve_replay(&ctx);
                if (!ok) {
                    char buf[128];
                    snprintf(buf, sizeof buf,
                             "solve_replay failed at difficulty %d, seed %u",
                             diff, SEEDS[s]);
                    check(false, d->id, buf);
                }
                runs++;
            }

        /* every tier of every hint must actually say something (§16.1) */
        for (int tier = 1; tier <= 3; tier++) {
            pz_ctx ctx = { 0 };
            ctx.seed = SEEDS[0];
            ctx.area = (Rectangle){ 170, 130, 940, 430 };
            d->init(&ctx);
            const char *h = d->hint(&ctx, tier);
            char buf[64];
            snprintf(buf, sizeof buf, "hint tier %d is empty", tier);
            check(h && strlen(h) > 16, d->id, buf);
        }

        printf("  ok    %-16s %2d fixture runs, 3 hint tiers\n", d->id, runs);
    }

    /* determinism: same seed, same instance. Compared through the only public
     * surface a puzzle has — its tier-3 hint, which names the answer. */
    for (int i = 0; i < n; i++) {
        const pz_def *d = puzzle_at(i);
        if (!d->init || !d->hint) continue;
        char first[256];
        pz_ctx a = { 0 };
        a.seed = 4242u;
        a.area = (Rectangle){ 170, 130, 940, 430 };
        d->init(&a);
        snprintf(first, sizeof first, "%s", d->hint(&a, 3));
        pz_ctx b = { 0 };
        b.seed = 4242u;
        b.area = a.area;
        d->init(&b);
        check(strcmp(first, d->hint(&b, 3)) == 0, d->id,
              "not deterministic for a fixed seed");
    }

    if (g_fail) {
        printf("test_puzzles: %d failure(s)\n", g_fail);
        return 1;
    }
    printf("test_puzzles: all green\n");
    return 0;
}
