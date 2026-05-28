#include "ballistic_engine.h"
#include <cmath>

struct TableEntry
{
    double dist;
    double qe;
    double tof;
    double drift;
};

//////////////////////////////////////////////////
// 🔥 TABLAS 105mm
//////////////////////////////////////////////////

static std::vector<TableEntry> table_105_C5 =
{
    {10000,82.5,16.6,3.6},
    {10500,87.5,17.4,3.8},
    {11000,92.6,18.2,4.0}
};

static std::vector<TableEntry> table_105_C6 =
{
    {10000,75.0,15.0,3.2},
    {10500,80.0,15.8,3.4},
    {11000,85.0,16.6,3.6}
};

//////////////////////////////////////////////////
// 🔥 TABLAS 155mm (ejemplo base)
//////////////////////////////////////////////////

static std::vector<TableEntry> table_155_M4 =
{
    {10000,45.0,25.0,5.0},
    {12000,50.0,28.0,6.0},
    {14000,55.0,32.0,7.0}
};

//////////////////////////////////////////////////
// 🔥 SELECTOR AUTOMÁTICO
//////////////////////////////////////////////////

static std::vector<TableEntry>* selectTable(const std::string& charge)
{
    // 105 mm
    if(charge=="C5") return &table_105_C5;
    if(charge=="C6") return &table_105_C6;

    // 155 mm
    if(charge=="M4") return &table_155_M4;

    // default
    return &table_105_C5;
}

//////////////////////////////////////////////////
// 🔥 INTERPOLACIÓN
//////////////////////////////////////////////////

BallisticResult BallisticEngine::getSolution(double dist, const std::string& charge)
{
    auto* table = selectTable(charge);

    if(dist <= table->front().dist)
        return {table->front().qe, table->front().tof, table->front().drift};

    if(dist >= table->back().dist)
        return {table->back().qe, table->back().tof, table->back().drift};

    for(size_t i = 0; i < table->size()-1; i++)
    {
        auto e1 = (*table)[i];
        auto e2 = (*table)[i+1];

        if(dist >= e1.dist && dist <= e2.dist)
        {
            double r = (dist - e1.dist)/(e2.dist - e1.dist);

            return {
                e1.qe + r*(e2.qe - e1.qe),
                e1.tof + r*(e2.tof - e1.tof),
                e1.drift + r*(e2.drift - e1.drift)
            };
        }
    }

    return {0,0,0};
}