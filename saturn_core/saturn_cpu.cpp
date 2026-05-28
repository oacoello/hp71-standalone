#include "saturn_cpu.h"
#include "saturn_memory.h"
#include "saturn_decoder.h"
#include <iostream>

SaturnCPU::SaturnCPU()
{
    PC = 0;
    sp = 0;

    flag_zero = false;
    flag_carry = false;

    memory = nullptr;

    for(int i=0;i<16;i++)
        stack[i] = 0;
}

void SaturnCPU::attach_memory(SaturnMemory* mem)
{
    memory = mem;
}

void SaturnCPU::reset()
{
    std::cout << "CPU RESET START" << std::endl;

    PC = 0;
    sp = 0;

    std::cout << "RESET PC = 0x" << std::hex << PC << std::endl;
}

uint8_t SaturnCPU::mem_read(uint32_t addr)
{
    if(!memory)
        return 0;

    return memory->read(addr);
}

void SaturnCPU::mem_write(uint32_t addr,uint8_t value)
{
    if(memory)
        memory->write(addr,value);
}

uint8_t SaturnCPU::read_nibble(uint32_t nib_addr)
{
    uint32_t byte_addr = nib_addr >> 1;

    uint8_t byte = mem_read(byte_addr);

    if(nib_addr & 1)
        return byte & 0x0F;
    else
        return (byte >> 4) & 0x0F;
}

void SaturnCPU::write_nibble(uint32_t nib_addr,uint8_t value)
{
    uint32_t byte_addr = nib_addr >> 1;

    uint8_t byte = mem_read(byte_addr);

    if(nib_addr & 1)
    {
        byte &= 0xF0;
        byte |= (value & 0x0F);
    }
    else
    {
        byte &= 0x0F;
        byte |= (value & 0x0F) << 4;
    }

    mem_write(byte_addr,byte);
}

uint32_t SaturnCPU::fetch_addr()
{
    uint32_t addr = 0;

    for(int i=0;i<5;i++)
    {
        uint8_t nib = read_nibble(PC);
        PC++;

        addr |= (nib << (i*4));
    }

    return addr;
}

void SaturnCPU::step()
{
    uint8_t op = read_nibble(PC);
    PC++;

    saturn_decode(*this,op);
}