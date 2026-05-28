#pragma once

#include <cstdint>

void saturn_field_copy(uint8_t* dst,uint8_t* src,int start,int end);
void saturn_field_clear(uint8_t* reg,int start,int end);
void saturn_field_add(uint8_t* dst,uint8_t* src,int start,int end);
void saturn_field_sub(uint8_t* dst,uint8_t* src,int start,int end);