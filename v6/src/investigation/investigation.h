#ifndef HD_INVESTIGATION_H
#define HD_INVESTIGATION_H
#include "art/artkit.h"

/* A chapter follows solved deductions, independently of the palette animation. */
int investigation_chapter(void);
const char *investigation_board(void);
const char *investigation_question(void);
int investigation_requirements(const char *board, char clues[][48], int cap);
bool investigation_ready(const char *board);
void investigation_summary(char *out, int cap);

typedef struct {
    const char *id, *name, *gate;
    float x, y;
} TownPlace;
int town_place_count(void);
const TownPlace *town_place(int i);
bool town_unlocked(const char *scene);

typedef struct {
    const char *scene, *label, *text;
    int doodle;
    float x, y;
} Observation;
typedef struct {
    const char *title, *question, *npc, *reward, *ending;
    Observation obs[2];
    const char *answer[3];
    int correct, decoration;
} SmallCase;
int small_case_count(void);
const SmallCase *small_case(int i);
bool small_case_seen(int i, int obs);
bool small_case_solved(int i);
bool small_case_observe(int i, int obs);
bool small_case_answer(int i, int answer);
int small_case_completed(void);

void casebook_open(int tab);
void casebook_draw(void);
/* 0 stays open, 1 closes, 2 travel, 3 open the current board */
int casebook_update(void);
const char *casebook_destination(void);
#endif
