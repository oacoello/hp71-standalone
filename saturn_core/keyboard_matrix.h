#pragma once

#include <cstdint>

class KeyboardMatrix
{

public:

    KeyboardMatrix();

    void press(int row,int col);
    void release(int row,int col);

    uint8_t read_row(int row);

private:

    bool matrix[8][10];

};