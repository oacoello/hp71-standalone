#pragma once

#include <cstdint>

void saturn_shift_left(uint8_t* reg,int start,int end);
void saturn_shift_right(uint8_t* reg,int start,int end);

void saturn_rotate_left(uint8_t* reg,int start,int end);
void saturn_rotate_right(uint8_t* reg,int start,int end);