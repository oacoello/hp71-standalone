#include "hp71_lcd.h"

HP71LCD::HP71LCD()
{
    screen="HP-71B BASIC READY";
}

void HP71LCD::set_text(const std::string& text)
{
    screen=text;
}

std::string HP71LCD::render()
{
    return screen;
}