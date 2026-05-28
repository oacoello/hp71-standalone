#include "ballistic_tables.h"
#include <vector>

static std::vector<TableEntry> table105 =
{
    {100,5.0,0.5},
    {200,9.5,0.9},
    {300,14.0,1.2},
    {400,18.5,1.5},
    {500,23.0,1.8},
    {600,27.5,2.1},
    {700,32.0,2.5},
    {800,36.5,2.8},
    {900,41.0,3.2},
    {1000,45.0,3.6}
};

static std::vector<TableEntry> table155 =
{
    {500,3.0,1.0},
    {1000,6.5,2.0},
    {1500,10.5,3.1},
    {2000,14.8,4.3},
    {2500,19.3,5.6},
    {3000,24.0,7.0},
    {3500,29.0,8.5},
    {4000,34.0,10.1}
};

static TableEntry interpolate(std::vector<TableEntry>& table,double range)
{
    for(size_t i=0;i<table.size()-1;i++)
    {
        if(range>=table[i].range && range<=table[i+1].range)
        {
            double r1=table[i].range;
            double r2=table[i+1].range;

            double f=(range-r1)/(r2-r1);

            TableEntry result;

            result.range=range;
            result.qe=table[i].qe + f*(table[i+1].qe-table[i].qe);
            result.tof=table[i].tof + f*(table[i+1].tof-table[i].tof);

            return result;
        }
    }

    return table.back();
}

TableEntry BallisticTables::interpolate105(double range)
{
    return interpolate(table105,range);
}

TableEntry BallisticTables::interpolate155(double range)
{
    return interpolate(table155,range);
}
