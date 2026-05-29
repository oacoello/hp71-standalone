#pragma once
#include <vector>
#include <string>

struct BallisticResult
{
    double qe;
    double tof;
    double drift;
};

class BallisticEngine
{
public:
    BallisticResult getSolution(double dist, const std::string& charge);
};