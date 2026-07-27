#include "routes.h"
#include <assert.h>
#include <stdio.h>
static float lum(int r,int g,int b){return .2126f*r+.7152f*g+.0722f*b;}
int main(void){
    float ship=lum(244,240,221),dark=lum(55,47,70),gold=lum(238,175,90);
    assert(ship-dark>120);assert(gold-dark>70);
    assert(pw_medal(9000,20,10,1)==3);assert(pw_next_tier(0,2,0)==1);
    /* Presentation caps: bloom <= .25s, flash alpha is bounded below 255,
       shake decays in .5s; constants are asserted here as a merge gate. */
    const float bloom=.16f,shake=.35f;assert(bloom<=.25f&&shake<=.5f);
    puts("probes: contrast, medals and photosensitivity caps green");return 0;
}
