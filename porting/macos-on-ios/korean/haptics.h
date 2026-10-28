#pragma once
#include <stdint.h>
void ngb_ios_haptics_prepare(unsigned port);
void ngb_ios_haptics_set(unsigned port,uint16_t low,uint16_t high);
void ngb_ios_haptics_active(int active);
