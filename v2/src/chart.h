/* Charts are authored in musical coordinates (bar, step) and resolved to
 * sample time as the sequencer publishes each bar's grid (§4, §5.1). */
#ifndef G_CHART_H
#define G_CHART_H

#include "common.h"
#include "sequencer.h"

#define CHART_MAX 128
#define PHRASE_BARS 4

typedef enum { NOTE_TAP = 0, NOTE_HOLD, NOTE_MARK } NoteKind;

typedef struct {
    int8_t   bar;         /* 0..PHRASE_BARS-1, relative to the phrase */
    uint8_t  step;        /* 0..15 */
    uint8_t  lane;        /* LANE_* */
    uint8_t  kind;
    uint8_t  dur_steps;   /* holds */
    uint8_t  resolved;
    uint8_t  judged;      /* Judgment */
    uint8_t  holding;
    uint8_t  tag;         /* microgame-defined (chord tone, sequence index...) */
    int8_t   midi;        /* chord tone for the PERFECT ding, -1 = default */
    uint64_t t;
    uint64_t t_end;
    float    off_ms;
} Note;

typedef struct {
    Note n[CHART_MAX];
    int  count;
    int  base_bar;                     /* absolute bar index of relative bar 0 */
    int  bars;
    uint64_t bar_start[PHRASE_BARS + 1];
    uint8_t  bar_known[PHRASE_BARS + 1];
    float    bar_spb[PHRASE_BARS + 1]; /* samples per beat, per bar */
} Chart;

typedef struct {
    int perfect, good, miss, extra;
    int judged;
} PhraseStats;

void  chart_reset(Chart *c, int base_bar, int bars);
Note *chart_add(Chart *c, int bar, int step, int lane, NoteKind kind, int dur_steps);
/* called for each published bar; resolves any notes living in that bar */
void  chart_resolve_bar(Chart *c, const BarInfo *bi);

Judgment chart_judge_offset(double off_samples, float win_scale);
/* nearest unjudged, resolved note on `lane` (lane<0 = any) within the GOOD
 * window; returns index or -1 */
int   chart_find_hit(Chart *c, int lane, uint64_t t, float win_scale);
/* mark every resolved note whose window has fully closed as MISS.
 * returns how many newly missed; indices reported through `out` (may be NULL) */
int   chart_expire(Chart *c, uint64_t now, float win_scale, int *out, int out_max);

bool  chart_default_success(const PhraseStats *s);
void  stats_add(PhraseStats *s, Judgment j);
int   stats_score(const PhraseStats *s);

#endif
