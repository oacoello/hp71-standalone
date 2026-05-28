#include "keyboard_matrix.h"

namespace Saturn
{

KeyboardMatrix::KeyboardMatrix()
{
    for(int i=0;i<8;i++)
        matrix[i] = 0xFF;
}

void KeyboardMatrix::press_key(uint8_t row,uint8_t col)
{
    matrix[row] &= ~(1 << col);
}

void KeyboardMatrix::release_key(uint8_t row,uint8_t col)
{
    matrix[row] |= (1 << col);
}

uint8_t KeyboardMatrix::read_row(uint8_t row)
{
    return matrix[row];
}

}