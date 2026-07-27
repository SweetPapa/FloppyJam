#ifndef PW_ROUTES_H
#define PW_ROUTES_H
typedef struct { const char *name,*subtitle; int base_medal; } PwStage;
extern const PwStage PW_STAGES[9];
int pw_medal(int score,int gates,int perfects,int survived);
int pw_next_tier(int tier,int medal,int secrets);
#endif
