#pragma once

#include <cstdint>

class SaturnCPU;

void saturn_alu_add(uint8_t* dst,uint8_t* src);
void saturn_alu_sub(uint8_t* dst,uint8_t* src);
void saturn_alu_clear(uint8_t* reg);
void saturn_alu_copy(uint8_t* dst,uint8_t* src);