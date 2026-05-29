#include "saturn_shift.h"

void saturn_shift_left(uint8_t* reg,int start,int end)
{

    for(int i=start;i<end;i++)
        reg[i] = reg[i+1];

    reg[end] = 0;

}

void saturn_shift_right(uint8_t* reg,int start,int end)
{

    for(int i=end;i>start;i--)
        reg[i] = reg[i-1];

    reg[start] = 0;

}

void saturn_rotate_left(uint8_t* reg,int start,int end)
{

    uint8_t first = reg[start];

    for(int i=start;i<end;i++)
        reg[i] = reg[i+1];

    reg[end] = first;

}

void saturn_rotate_right(uint8_t* reg,int start,int end)
{

    uint8_t last = reg[end];

    for(int i=end;i>start;i--)
        reg[i] = reg[i-1];

    reg[start] = last;

}