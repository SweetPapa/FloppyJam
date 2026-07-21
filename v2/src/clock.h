/* Host-time <-> sample-time mapping and latency calibration (§4.1). */
#ifndef G_CLOCK_H
#define G_CLOCK_H

#include "common.h"
#include "audio.h"

void   clock_reset(void);
/* call once per frame with a fresh snapshot and the frame's host timestamp */
void   clock_frame(const ClockSnap *snap, double host_time);
/* map a host timestamp to sample time, calibration applied */
double clock_map(double host_time);
double clock_map_raw(double host_time);

void   clock_set_calibration_ms(float ms);
float  clock_calibration_ms(void);

static inline float samples_to_ms(double samples) { return (float)(samples * 1000.0 / SAMPLE_RATE); }
static inline double ms_to_samples(float ms) { return (double)ms * SAMPLE_RATE / 1000.0; }

#endif
