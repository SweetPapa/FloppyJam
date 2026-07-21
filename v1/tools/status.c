#include "headless.h"
int main(void){HeadlessStatus s=sfbs_headless_verify();sfbs_status_json(&s);return s.failures?1:0;}
