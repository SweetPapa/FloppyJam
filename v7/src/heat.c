/* heat.c — Section 4 and §5.4. Small, pure, and the most tuned file here. */
#include "heat.h"

float vb_heat_speed(int heat) {
    int h = vb_clampi(heat, 0, VB_HEAT_MAX);
    float s = VB_V0;
    /* Iterated rather than powf: an exact, repeatable multiply chain. powf is
     * not guaranteed identical across libms, and this number decides where the
     * ball goes — §5.1 says the sim may not depend on that. */
    for (int i = 0; i < h; i++) s *= VB_HEAT_BASE;
    return s;
}

int vb_heat_up(int heat, int cap) {
    int top = (cap > 0 && cap < VB_HEAT_MAX) ? cap : VB_HEAT_MAX;
    return heat >= top ? top : heat + 1;
}

int vb_heat_down(int heat) { return heat > 0 ? heat - 1 : 0; }

int vb_heat_mercy(int heat) {
    return heat >= VB_MERCY_HEAT ? VB_MERCY_TICKS : 0;
}

float vb_heat_temp(int heat) {
    return vb_clampf((float)heat / (float)VB_HEAT_MAX, 0.0f, 1.0f);
}

int vb_heat_layers(int heat) {
    if (heat >= 12) return 4;
    if (heat >= 9)  return 3;
    if (heat >= 6)  return 2;
    if (heat >= 3)  return 1;
    return 0;
}
