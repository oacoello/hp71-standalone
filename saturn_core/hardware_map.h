#pragma once

#include <cstdint>

class SaturnMemory;
class HP71LCD;
class KeyboardMatrix;

class HardwareMap
{

public:

    HardwareMap();

    void attach_memory(SaturnMemory* mem);
    void attach_lcd(HP71LCD* lcd_dev);
    void attach_keyboard(KeyboardMatrix* kb);

    uint8_t read(uint32_t addr);
    void write(uint32_t addr,uint8_t value);

private:

    SaturnMemory* memory;
    HP71LCD* lcd;
    KeyboardMatrix* keyboard;

};