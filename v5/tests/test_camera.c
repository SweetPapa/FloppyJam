/* test_camera.c — the opening flyover must not stutter.
 *
 * The original bug: pullback() quantised the eye distance into 24 discrete
 * samples, so as the camera crossed the hole the eye snapped between steps
 * and the whole opening pan juddered. This measures the thing the eye
 * actually sees — frame-to-frame change in eye distance and position — and
 * fails if any single frame jumps far more than its neighbours.
 *
 *   make testcamera
 */
#include "../src/camera.h"
#include "../src/course.h"
#include <stdio.h>
#include <string.h>

static int failures = 0, checks = 0;

static void ok(int cond, const char *what, const char *detail)
{
    ++checks;
    printf("  %s %s%s%s\n", cond ? "ok  " : "FAIL", what,
           detail ? " — " : "", detail ? detail : "");
    if (!cond) ++failures;
}

static V3 cup_of(const BpWorld *w)
{
    int c = bp_course_cup(w);
    return (c < 0) ? v3zero() : v3(w->pockets[c].x, w->pockets[c].y, w->pockets[c].z);
}

/* Judder is not "the camera moved a lot", it is "the camera moved a lot MORE
 * than it did last frame". So measure the biggest frame-to-frame change in
 * step size, relative to the average step across the pan. Smooth easing sits
 * around 0.1; the quantised pullback spiked every time the eye crossed a
 * sample boundary. Only the moving part of the pan is measured — the settled
 * tail would otherwise drag the average to zero. */
static float judder(int hole, int mode, int frames, int orbit)
{
    BpWorld w;
    BpCam c;
    float steps[256];
    int n = 0, i;
    float sum = 0.0f, mean, worst = 0.0f;

    if (frames > 256) frames = 256;
    bp_course_build(&w, hole);
    bp_cam_init(&c, w.balls[0].p);
    bp_cam_set_mode(&c, mode, &w, w.balls[0].p, cup_of(&w));
    if (mode == CAM_FLYOVER) c.target = cup_of(&w);
    bp_cam_update(&c, &w, w.balls[0].p, v3zero(), 0.0f, 1.0f / 60.0f);

    for (i = 0; i < frames; ++i) {
        V3 prev = c.pos;
        if (orbit) bp_cam_orbit(&c, 0.02f, 0.0f);
        bp_cam_update(&c, &w, w.balls[0].p, v3zero(), 0.0f, 1.0f / 60.0f);
        steps[n] = v3len(v3sub(c.pos, prev));
        sum += steps[n];
        ++n;
    }
    mean = sum / (float)n;
    if (mean < 1e-7f) return 0.0f;
    for (i = 1; i < n; ++i) {
        float d = fabsf(steps[i] - steps[i - 1]) / mean;
        if (d > worst) worst = d;
    }
    return worst;
}

int main(void)
{
    int h;
    char buf[128];
    float worst_fly = 0.0f, worst_survey = 0.0f;
    int worst_fly_h = 0, worst_survey_h = 0;

    printf("BREAK PAR — camera suite\n\nopening flyover smoothness\n");
    for (h = 0; h < BP_NHOLES; ++h) {
        /* 150 frames = the 2.5 s pan, before it settles at the tee */
        float r = judder(h, CAM_FLYOVER, 150, 0);
        if (r > worst_fly) { worst_fly = r; worst_fly_h = h + 1; }
    }
    snprintf(buf, sizeof buf, "worst judder %.2f of a mean step, on hole %d",
             worst_fly, worst_fly_h);
    ok(worst_fly < 0.60f, "no hole judders during the opening pan", buf);

    printf("\nsurvey orbit smoothness\n");
    for (h = 0; h < BP_NHOLES; ++h) {
        float r = judder(h, CAM_SURVEY, 200, 1);   /* orbit past the rails */
        if (r > worst_survey) { worst_survey = r; worst_survey_h = h + 1; }
    }
    snprintf(buf, sizeof buf, "worst judder %.2f of a mean step, on hole %d",
             worst_survey, worst_survey_h);
    ok(worst_survey < 0.60f, "orbiting past walls does not pop the eye", buf);

    /* The eye must never end a frame inside geometry. It used to, routinely:
     * eye_dist was EASED toward the safe distance, so for a third of a second
     * after you orbited into a rail the camera was sitting inside it, and
     * posts were not tested at all — hole 17 has fifteen of them. Sweep every
     * hole at four pitches through a full revolution and count. */
    printf("\neye never enters geometry\n");
    {
        static const float PITCH[4] = { 6.0f, 26.0f, 52.0f, 79.0f };
        int inside = 0, worst_h = 0, p, z;
        for (h = 0; h < BP_NHOLES; ++h) {
            BpWorld w;
            bp_course_build(&w, h);
            for (p = 0; p < 4; ++p) {
                for (z = 0; z < 4; ++z) {
                    BpCam c;
                    int i;
                    bp_cam_init(&c, w.balls[0].p);
                    c.pitch = PITCH[p] * BP_DEG;
                    c.zoom = z;
                    bp_cam_update(&c, &w, w.balls[0].p, v3zero(), 0.0f, 1.0f / 60.0f);
                    for (i = 0; i < 180; ++i) {   /* a full turn, 2 deg a frame */
                        bp_cam_orbit(&c, 0.0349f, 0.0f);
                        bp_cam_update(&c, &w, w.balls[0].p, v3zero(), 0.0f, 1.0f / 60.0f);
                        if (bp_cam_eye_blocked(&c, &w)) {
                            if (!inside) worst_h = h + 1;
                            ++inside;
                        }
                    }
                }
            }
        }
        if (inside)
            snprintf(buf, sizeof buf, "%d frames inside, first on hole %d",
                     inside, worst_h);
        else
            snprintf(buf, sizeof buf, "clean across %d sampled frames",
                     BP_NHOLES * 4 * 4 * 180);
        ok(inside == 0, "orbiting never puts the eye inside geometry", buf);
    }

    /* A long frame must not teleport the camera: exponential smoothing is
     * frame-rate independent, the old dt*rate clamp was not. */
    {
        BpWorld w;
        BpCam a, b;
        int i;
        float d;
        bp_course_build(&w, 17);
        bp_cam_init(&a, w.balls[0].p);
        bp_cam_init(&b, w.balls[0].p);
        bp_cam_orbit(&a, 2.0f, 0.3f);
        bp_cam_orbit(&b, 2.0f, 0.3f);
        for (i = 0; i < 60; ++i)                    /* steady 60 fps */
            bp_cam_update(&a, &w, w.balls[0].p, v3zero(), 0.0f, 1.0f / 60.0f);
        for (i = 0; i < 6; ++i)                     /* same second, 10 fps */
            bp_cam_update(&b, &w, w.balls[0].p, v3zero(), 0.0f, 1.0f / 10.0f);
        d = v3len(v3sub(a.pos, b.pos));
        snprintf(buf, sizeof buf, "60 fps vs 10 fps end up %.3f m apart", d);
        ok(d < 0.05f, "camera settles to the same place at any frame rate", buf);
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
