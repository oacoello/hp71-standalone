#pragma once

#include <cstdint>

class SaturnCPU;

class SaturnTimer
{

public:

    SaturnTimer();

    void attach_cpu(SaturnCPU* cpu);

    void tick();

    uint8_t read(uint32_t addr);
    void write(uint32_t addr,uint8_t value);

private:

    SaturnCPU* cpu;

    uint32_t counter;
    uint32_t prescaler;

    bool interrupt_flag;

};