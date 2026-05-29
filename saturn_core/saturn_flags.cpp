#include "saturn_flags.h"

void saturn_update_zero(SaturnCPU& cpu,uint8_t* reg)
{

    bool zero = true;

    for(int i=0;i<16;i++)
    {

        if(reg[i] != 0)
        {
            zero = false;
            break;
        }

    }

    cpu.flag_zero = zero;

}

void saturn_set_carry(SaturnCPU& cpu,bool v)
{
    cpu.flag_carry = v;
}

bool saturn_get_carry(SaturnCPU& cpu)
{
    return cpu.flag_carry;
}