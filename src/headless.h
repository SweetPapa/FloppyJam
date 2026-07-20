#ifndef SFBS_HEADLESS_H
#define SFBS_HEADLESS_H
#include <stdint.h>
typedef struct { int checks,failures,modes_completed,deterministic_pairs,seeds_checked; uint64_t digest; const char *first_failure; } HeadlessStatus;
HeadlessStatus sfbs_headless_verify(void);
void sfbs_status_json(const HeadlessStatus*);
#endif
