#include "map_model.h"

MapModel::MapModel()
{
    gridSystem = 0;
    spheroid = 1;
    mapSheet = "NONE";
}

std::string MapModel::openMenu()
{
    return
        "MAP MODEL\n"
        "1 GRID\n"
        "2 SPHEROID\n"
        "3 MAP SHEET\n"
        "4 COORD CONV\n"
        "X EXIT";
}

std::string MapModel::handleInput(const std::string& cmd)
{

    if(cmd=="1")
    {
        gridSystem = 16;
        return "GRID: UTM ZONE 16";
    }

    if(cmd=="2")
    {
        spheroid = 1;
        return "SPHEROID: WGS84";
    }

    if(cmd=="3")
    {
        mapSheet = "HONDURAS";
        return "MAP SHEET SET";
    }

    if(cmd=="4")
    {
        return "COORD CONVERSION READY";
    }

    if(cmd=="X")
    {
        return "MAIN";
    }

    return "MAP MODEL";
}