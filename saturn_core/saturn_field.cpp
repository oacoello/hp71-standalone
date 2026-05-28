#include "saturn_field.h"

void saturn_field_copy(uint8_t* dst,uint8_t* src,int start,int end)
{

    for(int i=start;i<=end;i++)
        dst[i] = src[i];

}

void saturn_field_clear(uint8_t* reg,int start,int end)
{

    for(int i=start;i<=end;i++)
        reg[i] = 0;

}

void saturn_field_add(uint8_t* dst,uint8_t* src,int start,int end)
{

    int carry = 0;

    for(int i=end;i>=start;i--)
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

void saturn_field_sub(uint8_t* dst,uint8_t* src,int start,int end)
{

    int borrow = 0;

    for(int i=end;i>=start;i--)
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