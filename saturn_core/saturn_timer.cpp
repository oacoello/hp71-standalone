#include "saturn_timer.h"
#include "saturn_cpu.h"

SaturnTimer::SaturnTimer()
{

    cpu = nullptr;

    counter = 0;
    prescaler = 0;

    interrupt_flag = false;

}

void SaturnTimer::attach_cpu(SaturnCPU* c)
{
    cpu = c;
}

void SaturnTimer::tick()
{

    prescaler++;

    if(prescaler >= 1000)
    {

        prescaler = 0;

        counter++;

        if(counter >= 1000)
        {

            counter = 0;

            interrupt_flag = true;

        }

    }

}

uint8_t SaturnTimer::read(uint32_t addr)
{

    switch(addr)
    {

        case 0:
            return counter & 0xF;

        case 1:
            return (counter >> 4) & 0xF;

        case 2:
            return interrupt_flag ? 1 : 0;

    }

    return 0;

}

void SaturnTimer::write(uint32_t addr,uint8_t value)
{

    switch(addr)
    {

        case 0:
            counter = 0;
        break;

        case 1:
            interrupt_flag = false;
        break;

    }

}