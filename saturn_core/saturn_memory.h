#pragma once

#include <vector>
#include <cstdint>

class KeyboardMatrix;

class SaturnMemory
{

public:

    SaturnMemory();

    bool load_rom(const char* filename);

    uint8_t read(uint32_t addr);

    void write(uint32_t addr,uint8_t value);

    void attach_keyboard(KeyboardMatrix* kb);

private:

    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;

    KeyboardMatrix* keyboard;

};