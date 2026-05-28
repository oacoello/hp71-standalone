#include "saturn_memory.h"
#include "keyboard_matrix.h"

#include <fstream>
#include <iostream>

SaturnMemory::SaturnMemory()
{
    rom.resize(0x100000);
    ram.resize(0x100000);

    keyboard = nullptr;
}

bool SaturnMemory::load_rom(const char* filename)
{
    std::ifstream f(filename,std::ios::binary);

    if(!f)
        return false;

    f.read((char*)rom.data(),rom.size());

    std::cout << "ROM loaded" << std::endl;

    return true;
}

void SaturnMemory::attach_keyboard(KeyboardMatrix* kb)
{
    keyboard = kb;
}

uint8_t SaturnMemory::read(uint32_t addr)
{

    // ROM
    if(addr < rom.size())
        return rom[addr];

    // RAM
    if(addr >= 0x80000 && addr < 0x90000)
        return ram[addr - 0x80000];

    // teclado aún no implementado
    // la ROM puede leer aquí pero devolvemos 0
    if(addr == 0x60000)
        return 0;

    return 0;
}

void SaturnMemory::write(uint32_t addr,uint8_t value)
{

    if(addr >= 0x80000 && addr < 0x90000)
        ram[addr - 0x80000] = value;

}