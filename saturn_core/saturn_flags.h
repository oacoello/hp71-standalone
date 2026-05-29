#pragma once

#include "saturn_cpu.h"

void saturn_update_zero(SaturnCPU& cpu,uint8_t* reg);
void saturn_set_carry(SaturnCPU& cpu,bool v);
bool saturn_get_carry(SaturnCPU& cpu);