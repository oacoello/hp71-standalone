#pragma once
#include <string>
#include <vector>
#include "ballistic_engine.h"

struct Gun
{
    double e;
    double n;
    double alt;
    double dir; // 🔥 AGREGAR ESTO
};

class BasicEngine
{
public:
    std::string execute(const std::string& cmd);
    std::string resetData();
private:

    //////////////////////////////////////////////////
    // ESTADO GENERAL
    //////////////////////////////////////////////////
    std::string current_menu = "MAIN";
    int input_stage = 0;

    //////////////////////////////////////////////////
    // POSICIONES
    //////////////////////////////////////////////////
    double cob_alt = 0;
    double cob_n = 0;
    double cob_e = 0;

    double tgt_alt = 0;
    double tgt_n = 0;
    double tgt_e = 0;

    double obs_alt = 0;
    double obs_n = 0;
    double obs_e = 0;

    static int obs_id;
    static double obs_gz;

    //////////////////////////////////////////////////
    // MET / MAP
    //////////////////////////////////////////////////
    double wind_dir = 0;
    double wind_speed = 0;
    double temperature = 0;

    double map_e_max = 0;
    double map_e_min = 0;
    double map_n_max = 0;
    double map_n_min = 0;
    double map_gz = 0;
    std::string map_spher = "";

    //////////////////////////////////////////////////
    // AMMO / MISSION
    //////////////////////////////////////////////////
    std::string ammo_proj_prop = "";
    std::string ammo_proj_lot = "";
    double ammo_proj_wt = 0;

    std::string mission_type = "";

    //////////////////////////////////////////////////
    // FUZE / CORR
    //////////////////////////////////////////////////
    bool fuze_time_mode = false;
    double hob = 0;

    double ud_corr = 0;
    bool ud_active = false;

    //////////////////////////////////////////////////
    // BALLISTIC
    //////////////////////////////////////////////////
    BallisticEngine ballistic;

    //////////////////////////////////////////////////
    // MULTI GUN
    //////////////////////////////////////////////////
    std::vector<Gun> guns;
    int gun_expected_count = 0;
    int gun_input_index = 0;
    int gun_input_stage = 0;
    Gun gun_temp;

    //////////////////////////////////////////////////
    // FUNCIONES
    //////////////////////////////////////////////////
    std::string mainMenu();
    std::string fireMenu();
    std::string afuMenu();
    std::string ammoMenu();

    std::string fireMission();

    std::string cobMenu(const std::string& input);
    std::string targetMenu(const std::string& input);
    std::string obsMenu(const std::string& input);
    std::string metMenu(const std::string& input);
    std::string mapModelHandler(const std::string& input);
    std::string ammoFileHandler(const std::string& input);

    std::string handleGuns(const std::string& cmd);
    std::string fireAllGuns();
};