#include "saturn_decoder.h"
#include "saturn_cpu.h"

void saturn_decode(SaturnCPU& cpu, uint8_t op)
{

    switch(op)
    {

        // BRANCH
        case 0x2:
        {
            uint32_t addr = cpu.fetch_addr();
            cpu.PC = addr;
        }
        break;

        // CALL
        case 0x3:
        {
            uint32_t addr = cpu.fetch_addr();

            cpu.stack[cpu.sp++] = cpu.PC;

            if(cpu.sp >= 16)
                cpu.sp = 15;

            cpu.PC = addr;
        }
        break;

        // RETURN
        case 0x4:
        {
            if(cpu.sp > 0)
                cpu.PC = cpu.stack[--cpu.sp];
        }
        break;

        // NOP
        case 0x0:
        break;

        // SIMPLE JUMP (boot ROM usa esto)
        case 0x5:
        {
            uint32_t addr = cpu.fetch_addr();
            cpu.PC = addr;
        }
        break;

        default:
        break;

    }

}