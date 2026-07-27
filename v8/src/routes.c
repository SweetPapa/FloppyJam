#include "routes.h"
const PwStage PW_STAGES[9]={
{"FIRST LIGHT","Wake the sleeping arches",2400},{"BROKEN CHOIR","The ruins learn your rhythm",3200},
{"THE GATEKEEPER","A fortress closes its fist",4200},{"GLASS CURRENT","Climb where the wind remembers",4800},
{"VIOLET REACH","Hunt the road above the sun",5600},{"STORM SHEPHERD","Ride the breath of thunder",6500},
{"THE QUIET KNIVES","Every opening is a promise",7400},{"LAST AURORA","Your shadow knows the route",8200},
{"MIRROR ARMADA","One heartbeat against the sky",9400}};
int pw_medal(int score,int gates,int perfects,int survived){int v=score+gates*30+perfects*80+(survived?1000:0);return v>8000?3:v>4800?2:v>2200?1:0;}
int pw_next_tier(int tier,int medal,int secrets){if((medal>=2||secrets)&&tier<2)return tier+1;if(medal==0&&tier>0)return tier-1;return tier;}
