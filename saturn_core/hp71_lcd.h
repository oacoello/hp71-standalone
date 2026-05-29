#pragma once
#include <string>

class HP71LCD
{

public:

    HP71LCD();

    void set_text(const std::string& text);

    std::string render();

private:

    std::string screen;

};