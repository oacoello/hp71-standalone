#pragma once

struct TableEntry
{
    double range;
    double qe;
    double tof;
};

class BallisticTables
{
public:

    static TableEntry interpolate105(double range);
    static TableEntry interpolate155(double range);

    // =========================
    // END OF MISSION (EOM)
    // =========================

    bool endOfMission = false;

    std::string handleEndOfMission();
    void resetMissionState();
};
