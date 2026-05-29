#include "keyboard_matrix.h"

KeyboardMatrix::KeyboardMatrix()
{

    for(int r=0;r<8;r++)
        for(int c=0;c<10;c++)
            matrix[r][c] = false;

}

void KeyboardMatrix::press(int row,int col)
{

    if(row < 0 || row >= 8)
        return;

    if(col < 0 || col >= 10)
        return;

    matrix[row][col] = true;

}

void KeyboardMatrix::release(int row,int col)
{

    if(row < 0 || row >= 8)
        return;

    if(col < 0 || col >= 10)
        return;

    matrix[row][col] = false;

}

uint8_t KeyboardMatrix::read_row(int row)
{

    if(row < 0 || row >= 8)
        return 0xFF;

    uint8_t value = 0;

    for(int c=0;c<8;c++)
    {
        if(matrix[row][c])
            value |= (1 << c);
    }

    return value;

}