#pragma once
#include <stdint.h>

namespace Saturn
{

class KeyboardMatrix
{
public:

    KeyboardMatrix();

    void press_key(uint8_t row,uint8_t col);
    void release_key(uint8_t row,uint8_t col);

    uint8_t read_row(uint8_t row);

private:

    uint8_t matrix[8];

};

}