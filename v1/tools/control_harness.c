#include "control.h"
int main(void){ControlStatus s=sfbs_control_verify();sfbs_control_json(&s);return s.failures?1:0;}
