#include "saturn_alu.h"

void saturn_alu_add(uint8_t* dst,uint8_t* src)
{

    int carry = 0;

    for(int i=15;i>=0;i--)
    {

        int v = dst[i] + src[i] + carry;

        if(v >= 10)
        {
            v -= 10;
            carry = 1;
        }
        else
        {
            carry = 0;
        }

        dst[i] = v & 0xF;

    }

}

void saturn_alu_sub(uint8_t* dst,uint8_t* src)
{

    int borrow = 0;

    for(int i=15;i>=0;i--)
    {

        int v = dst[i] - src[i] - borrow;

        if(v < 0)
        {
            v += 10;
            borrow = 1;
        }
        else
        {
            borrow = 0;
        }

        dst[i] = v & 0xF;

    }

}

void saturn_alu_clear(uint8_t* reg)
{

    for(int i=0;i<16;i++)
        reg[i] = 0;

}

void saturn_alu_copy(uint8_t* dst,uint8_t* src)
{

    for(int i=0;i<16;i++)
        dst[i] = src[i];

}