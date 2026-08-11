#pragma once
#include <string>
#include <vector>
#include "ballistic_engine.h"

//////////////////////////////////////////////////
// ENUM DE MENUS — reemplaza strings por integers
//////////////////////////////////////////////////
enum Menu
{
    MENU_MAIN,
    MENU_FM,
    MENU_AFU,
    MENU_AMMO,
    MENU_MET,
    MENU_MAP_MODEL,
    MENU_OBS,
    MENU_TARGET,
    MENU_ART_TYPE,
    MENU_COB_QTY,
    MENU_COB_BASE,
    MENU_COB_GB_E,
    MENU_COB_GB_N,
    MENU_COB_GB_ALT,
    MENU_COB_AZ_LAY,
    MENU_COB_REF_DEF,
    MENU_COB_DIR,
    MENU_COB_DIST,
    MENU_COB_IV,
    MENU_CHG_EDIT,
    MENU_FM1_TGT,
    MENU_FM1_GRID,
    MENU_FM1_TRANSPORT,
    MENU_FM1_POLAR,
    MENU_FM1_BASE_PIECE,
    MENU_FM4_LR,
    MENU_FM4_AD,
    MENU_FM4_UD,
    MENU_FM4_NEXT,
    MENU_REG,
    MENU_REG_BASE_PIECE,
    MENU_SHEAF,
    MENU_SHEAF_WIDTH,
    MENU_SHIFT_PREV_DIR,
    MENU_SHIFT_PREV_LR,
    MENU_SHIFT_PREV_AD,
    MENU_SHIFT_PREV_UD,
    MENU_SHIFT_DIR,
    MENU_SHIFT_LR,
    MENU_SHIFT_AD,
    MENU_SHIFT_UD,
    MENU_SHIFT_ANGLE,
    MENU_INST_DIR,
    MENU_INST_LR_SHIFT,
    MENU_INST_AD_SHIFT,
    MENU_INST_UD_SHIFT,
    MENU_INST_PREV_DIR,
    MENU_INST_PREV_LR,
    MENU_INST_PREV_AD,
    MENU_INST_PREV_UD,
    MENU_INST_ANGLE_T,
    MENU_COMP_CORR,
    MENU_TIME_REG,
    MENU_TIME_REG_INPUT,
    MENU_TIME_REG_FUZE,
    MENU_TIME_REG_HOB,
    MENU_UD_CORR,
    MENU_SHIFT,
    MENU_DF_CORR,
    MENU_COUNT // sentinel para validación
};

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

    // 🔥 OBS públicos porque basic_engine.cpp los usa como BasicEngine::obs_id / BasicEngine::obs_gz
    static int obs_id;
    static double obs_gz;

private:

    //////////////////////////////////////////////////
    // ESTADO GENERAL
    //////////////////////////////////////////////////
    Menu current_menu = MENU_MAIN;
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

    //////////////////////////////////////////////////
    // MET / MAP
    //////////////////////////////////////////////////
    double wind_dir = 0;
    double wind_speed = 2.5;  // Honduras average
    double temperature = 28.0;  // Honduras average

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