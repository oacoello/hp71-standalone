#ifndef MAP_MODEL_H
#define MAP_MODEL_H

#include <string>

class MapModel
{

public:

    MapModel();

    std::string openMenu();
    std::string handleInput(const std::string& cmd);

private:

    int gridSystem;
    int spheroid;
    std::string mapSheet;

};

#endif