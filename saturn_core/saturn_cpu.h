#pragma once

#include <cstdint>

class SaturnMemory;

class SaturnCPU
{

public:

    SaturnCPU();

    void attach_memory(SaturnMemory* mem);

    void reset();

    void step();

    uint8_t mem_read(uint32_t addr);
    void mem_write(uint32_t addr,uint8_t value);

    uint8_t read_nibble(uint32_t nib_addr);
    void write_nibble(uint32_t nib_addr,uint8_t value);

    uint32_t fetch_addr();

public:

    uint32_t PC;

    uint32_t stack[16];
    int sp;

    bool flag_zero;
    bool flag_carry;

private:

    SaturnMemory* memory;

};