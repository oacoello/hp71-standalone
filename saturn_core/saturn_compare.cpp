#include "saturn_compare.h"

bool saturn_is_zero(uint8_t* reg)
{
    for(int i=0;i<16;i++)
    {
        if(reg[i] != 0)
            return false;
    }

    return true;
}

bool saturn_equal(uint8_t* a,uint8_t* b)
{
    for(int i=0;i<16;i++)
    {
        if(a[i] != b[i])
            return false;
    }

    return true;
}

bool saturn_greater(uint8_t* a,uint8_t* b)
{
    for(int i=0;i<16;i++)
    {

        if(a[i] > b[i])
            return true;

        if(a[i] < b[i])
            return false;

    }

    return false;
}