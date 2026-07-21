#include "clock.h"

/* sample_time ~= host_time * SAMPLE_RATE + offset, offset EMA-smoothed so a
 * single jittery callback can't move a judgment by a frame. */
static double s_offset = 0.0;
static int    s_locked = 0;
static float  s_calib_ms = 0.0f;

void clock_reset(void) { s_locked = 0; s_offset = 0.0; }

void clock_frame(const ClockSnap *snap, double host_time)
{
    double predicted = host_time * (double)SAMPLE_RATE;
    double observed = (double)snap->sample_clock;
    double off = observed - predicted;
    if (!s_locked) {
        s_offset = off;
        s_locked = 1;
        return;
    }
    /* A big jump means the transport restarted; relock instead of crawling. */
    if (fabs(off - s_offset) > (double)SAMPLE_RATE * 0.25) {
        s_offset = off;
        return;
    }
    s_offset += (off - s_offset) * 0.08;   /* EMA */
}

double clock_map_raw(double host_time)
{
    return host_time * (double)SAMPLE_RATE + s_offset;
}

double clock_map(double host_time)
{
    return clock_map_raw(host_time) - ms_to_samples(s_calib_ms);
}

void  clock_set_calibration_ms(float ms) { s_calib_ms = g_clampf(ms, -200.0f, 200.0f); }
float clock_calibration_ms(void) { return s_calib_ms; }
