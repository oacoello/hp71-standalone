#include "basic_engine.h"

int BasicEngine::obs_id = 0;
double BasicEngine::obs_gz = 0;

#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <map>
#include <tuple>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <thread>
#include <chrono>
#endif

#include <algorithm>
#include <cctype>

#ifndef _WIN32
#include <unistd.h>
#endif

static std::string normStr(std::string s)
{

    for(char& c : s)
    {
        if(c == '\n' || c == '\r' || c == '\t')
            c = ' ';
    }

    while(!s.empty() && s.back() == ' ')
        s.pop_back();

    size_t i = 0;
    while(i < s.size() && s[i] == ' ')
        ++i;

    if(i >= s.size())
        return "";

    return s.substr(i);
}

struct Row
{
    double d;
    double qe;
    double tof;
    double drift;
    double angle;   
    double vterm;   
};

struct AmmoKey
{
    std::string artillery;
    std::string proj;
    std::string chg;

    bool operator<(const AmmoKey& o) const
    {
        return std::tie(artillery, proj, chg) <
               std::tie(o.artillery, o.proj, o.chg);
    }
};

static std::map<AmmoKey, std::vector<Row>> firingTables;

static void loadTablesFromCSV(const std::string& filename)
{
    firingTables.clear();

    std::ifstream file(filename);
    std::string line;

    if(!file.is_open())
    {
        std::cout << "ERROR: No se pudo abrir tables.csv\n";
        return;
    }

    auto trim = [](std::string s) -> std::string
    {
    while(!s.empty() && (s.back()=='\r' || s.back()=='\n' || s.back()==' ' || s.back()=='\t'))
        s.pop_back();

    size_t i = 0;
    while(i < s.size() && (s[i]==' ' || s[i]=='\t'))
        ++i;

    return s.substr(i);
    };

    std::getline(file, line); 

    while(std::getline(file, line))
    {
        if(line.empty()) continue;

        std::stringstream ss(line);
        std::string token;

        AmmoKey key;
        Row row;

        std::getline(ss, token, ','); key.artillery = trim(token);
        std::getline(ss, token, ','); key.proj = trim(token);
        std::getline(ss, token, ','); key.chg = trim(token);

        std::getline(ss, token, ','); row.d = std::stod(trim(token));
        std::getline(ss, token, ','); row.qe = std::stod(trim(token));
        std::getline(ss, token, ','); row.tof = std::stod(trim(token));
        std::getline(ss, token, ','); row.drift = std::stod(trim(token));

        row.angle = 0.0;
        row.vterm = 0.0;
        if(std::getline(ss, token, ','))
        {
            try { row.angle = std::stod(trim(token)); } catch(...) {}
        }
        if(std::getline(ss, token, ','))
        {
            try { row.vterm = std::stod(trim(token)); } catch(...) {}
        }

        firingTables[key].push_back(row);
    }

    for(auto& it : firingTables)
    {
        std::sort(it.second.begin(), it.second.end(),
            [](const Row& a,const Row& b){ return a.d < b.d; });
    }

    int warnings = 0;
    for(const auto& it : firingTables)
    {
        const AmmoKey& key = it.first;
        const std::vector<Row>& table = it.second;

        for(size_t i = 0; i < table.size(); i++)
        {

            if(table[i].qe <= 0)
            {
                std::cout << "WARNING: " << key.artillery << " " << key.proj << " " << key.chg
                         << " D=" << table[i].d << " QE=" << table[i].qe << " (<=0)\n";
                warnings++;
            }

            if(table[i].tof <= 0)
            {
                std::cout << "WARNING: " << key.artillery << " " << key.proj << " " << key.chg
                         << " D=" << table[i].d << " TOF=" << table[i].tof << " (<=0)\n";
                warnings++;
            }

            if(i > 0 && table[i].qe < table[i-1].qe - 1.0)
            {
                std::cout << "WARNING: " << key.artillery << " " << key.proj << " " << key.chg
                         << " D=" << table[i].d << " QE=" << table[i].qe 
                         << " < anterior " << table[i-1].qe << "\n";
                warnings++;
            }

            if(i > 0 && table[i].tof < table[i-1].tof - 0.1)
            {
                std::cout << "WARNING: " << key.artillery << " " << key.proj << " " << key.chg
                         << " D=" << table[i].d << " TOF=" << table[i].tof 
                         << " < anterior " << table[i-1].tof << "\n";
                warnings++;
            }
        }
    }

    if(warnings > 0)
    {
        std::cout << "TOTAL WARNINGS: " << warnings << "\n";
    }

    std::cout << "TABLAS CARGADAS DESDE CSV\n";
    std::cout << "SIZE = " << firingTables.size() << "\n";
    std::cout << "TOTAL TABLAS: " << firingTables.size() << "\n";

    for(const auto& it : firingTables)
    {
        std::cout << "ART=" << it.first.artillery
                << " PROJ=" << it.first.proj
                << " CHG=" << it.first.chg << "\n";
    }
}

struct TargetData
{
    double e = 0;
    double n = 0;
    double alt = 0;
};

static std::map<int, TargetData> targets;
static int current_knpt = 0;
static std::string current_tgt_indicator = "";

#define PI 3.14159265358979323846

static int fire_phase = 0;

static double gb_e = 0.0;
static double gb_n = 0.0;
static double gb_alt = 0.0;
static double az_lay = 0.0;

static int cob_expected_qty = 0;
static int cob_current_index = 0;

static double temp_dir = 0.0;
static double temp_dist = 0.0;
static double temp_iv = 0.0;

static int base_piece_index = 0;

static double sheaf_width = 60.0; 

static bool all_guns_command = false; 

double map_e_max = 0;
double map_e_min = 0;
double map_n_max = 0;
double map_n_min = 0;
double map_gz = 0;
std::string map_spher = "";

static int def_base = 3200;
static std::string artillery_type = "155";

static std::string ammo_proj_prop = "";
static std::string ammo_proj_lot = "";

struct ObserverData
{
    int id = 0;
    double e = 0;
    double n = 0;
    double alt = 0;
    double gz = 0;
};

static std::map<int, ObserverData> observers;

static int obs_expected_qty = 1;
static int obs_current_index = 1;

static double obs_alt = 0;
static double obs_e = 0;
static double obs_n = 0;

static std::vector<std::string> main_inputs;

static std::vector<std::string> last_inputs;

struct ShotLog
{
    std::string label;
    std::string inputs;
    std::string fire;
    std::string timestamp;
};

static std::vector<ShotLog> mission_log;

std::string drPrefix(const std::string& menu,const std::string& text)
{
    if(menu=="MAIN" || menu=="FM")
        return "DR EVIL\n"+text;
    return text;
}

static bool mission_active = false;

static bool manual_chg_enabled = false;
static std::string manual_chg_value = "";
static bool chg_allowed = false;
static bool chg_edit_mode = false;
static bool chg_wait_value = false;
static std::string last_solution = "";

static std::vector<double> gun_dirs;
static int mission_counter = 1;
static bool ffe_mode = false;
static bool ammo_input_active = false;
bool initializing = false;

static std::string sheaf_mode = "CONV";

static int fm1_tgt_method = 0;   
static int fm1_temp_knpt = 0;

static double fm1_temp_e = 0.0;
static double fm1_temp_n = 0.0;
static double fm1_temp_alt = 0.0;
static bool fm1_base_def_reverse = false;
static bool active_fm1_def_reverse = false;
static bool active_fm1_transport_qe_shape = false;

static bool fm1_grid_clean_fire_pending = false;
static double fm1_grid_reg_dist_backup = 0.0;
static double fm1_grid_reg_def_backup = 0.0;
static double fm1_grid_df_corr_backup = 0.0;
static double fm1_grid_time_reg_correction_backup = 0.0;
static bool fm1_grid_ud_active_backup = false;
static double fm1_grid_ud_corr_backup = 0.0;

static double fm1_pol_az = 0.0;
static double fm1_pol_dist = 0.0;

static char fm1_pol_ud = 'N';   
static double fm1_pol_ud_val = 0.0;

static int fm1_from_knpt = 0;

static double fm1_tr_az = 0.0;

static char fm1_tr_lr = 'N';   
static double fm1_tr_lr_val = 0.0;

static char fm1_tr_ad = 'N';   
static double fm1_tr_ad_val = 0.0;

static char fm1_tr_ud = 'N';   
static double fm1_tr_ud_val = 0.0;

bool boot_mode = true;
static double reg_dist = 0.0;
static double reg_def = 0.0;
static double df_corr = 0.0;
static double time_reg_correction = 0.0;

static double shift_lr = 0.0;
static double shift_ad = 0.0;
static double shift_ud = 0.0;
static double last_dist_solution = 0.0;
static double last_def_solution = 0.0;
static double last_qe_solution = 0.0;

static double inst_prev_dir = 0.0;
static double inst_prev_lr = 0.0;
static double inst_prev_ad = 0.0;
static double inst_prev_ud = 0.0;

static double inst_new_dir = 0.0;
static double inst_angle_t = 0.0;
static double inst_last_dir = 0.0;

static double inst_lr_shift = 0.0;
static double inst_ad_shift = 0.0;
static double inst_ud_shift = 0.0;

static std::string shift_prev_dir = "N";
static double shift_prev_lr = 0.0;
static double shift_prev_ad = 0.0;
static double shift_prev_ud = 0.0;

static std::string shift_new_dir = "N";
static double shift_angle = 0.0;

static bool reg_data_available = false;

static int last_knpt = 0;
static std::string last_proj = "";
static std::string last_lot = "";
static bool last_fuze_tia = false;
static double last_reg_rg = 0.0;
static double last_reg_def = 0.0;
static bool time_reg_fuze_temporary = false;

static bool interp(const std::vector<Row>& t,double d,double& qe,double& tof,double& drift)
{
    if(t.empty()) return false;

    if(d < t.front().d || d > t.back().d)
        return false;

    for(size_t i = 1; i < t.size(); i++)
    {
        if(d <= t[i].d)
        {
            const Row& a = t[i-1];
            const Row& b = t[i];

            double f = (d - a.d) / (b.d - a.d);

            if(i + 1 < t.size())
            {
                const Row& c = t[i+1];
                double qe_quad = a.qe + f*(b.qe - a.qe) + f*(f-1)/2.0 * (a.qe - 2.0*b.qe + c.qe);
                double tof_quad = a.tof + f*(b.tof - a.tof) + f*(f-1)/2.0 * (a.tof - 2.0*b.tof + c.tof);
                double drift_quad = a.drift + f*(b.drift - a.drift) + f*(f-1)/2.0 * (a.drift - 2.0*b.drift + c.drift);

                if(qe_quad > 0 && tof_quad > 0)
                {
                    qe = qe_quad;
                    tof = tof_quad;
                    drift = drift_quad;
                    return true;
                }
            }

            qe = a.qe + f * (b.qe - a.qe);
            tof = a.tof + f * (b.tof - a.tof);
            drift = a.drift + f * (b.drift - a.drift);

            return true;
        }
    }

    return false;
}

static bool solveAuto(const std::string& proj,const std::string& lot,double dist,
                     std::string& chg,double& qe,double& tof,double& drift)
{
    bool found = false;

    for(auto it = firingTables.begin(); it != firingTables.end(); ++it)
    {
        const AmmoKey& key = it->first;
        const std::vector<Row>& table = it->second;

        if(normStr(key.artillery) != normStr(artillery_type))
            continue;

        std::string proj_combined = normStr(proj) + normStr(lot);
        bool has_lot = !normStr(lot).empty();
        std::string key_proj = normStr(key.proj);
        std::string norm_proj = normStr(proj);

        bool proj_match_direct = (key_proj == norm_proj);
        bool proj_match_combined = has_lot && (key_proj == proj_combined);

        bool proj_match_suffix = false;
        if(!proj_match_direct && !proj_match_combined)
        {
            if((int)norm_proj.size() <= 3 && (int)key_proj.size() > (int)norm_proj.size())
            {
                std::string suffix = key_proj.substr(key_proj.size() - norm_proj.size());
                proj_match_suffix = (suffix == norm_proj);
            }
        }

        if(has_lot)
        {
            if(!proj_match_combined && !proj_match_direct && !proj_match_suffix)
                continue;
        }
        else
        {
            if(!proj_match_direct && !proj_match_suffix)
                continue;
        }

        double q,t,d;

        if(!interp(table, dist, q, t, d))
            continue;

        if(q <= 0 || t <= 0)
        {

            continue;
        }

        if(q > 1200)
        {

            continue;
        }

        if(t > 120)
        {

            continue;
        }

        double expected_tof_min = dist / 1000.0 * 3.0;
        double expected_tof_max = dist / 1000.0 * 8.0;
        if(t < expected_tof_min || t > expected_tof_max)
        {

            continue;
        }

        double candidate_qe = q;
        double candidate_tof = t;
        double candidate_drift = d;
        std::string candidate_chg = key.chg;

        if(!found || candidate_chg < chg || (candidate_chg == chg && candidate_qe < qe))
        {
            qe = candidate_qe;
            tof = candidate_tof;
            drift = candidate_drift;
            chg = candidate_chg;
            found = true;
        }
    }

    return found;
}

static bool solveByCharge(const std::string& proj,const std::string& lot,double dist,const std::string& requested_chg,
                         std::string& chg,double& qe,double& tof,double& drift)
{
    for(auto it = firingTables.begin(); it != firingTables.end(); ++it)
    {
        const AmmoKey& key = it->first;
        const std::vector<Row>& table = it->second;

        if(normStr(key.artillery) != normStr(artillery_type))
            continue;

        std::string proj_combined2 = normStr(proj) + normStr(lot);
        bool has_lot2 = !normStr(lot).empty();
        std::string key_proj2 = normStr(key.proj);
        std::string norm_proj2 = normStr(proj);
        bool match_direct2 = (key_proj2 == norm_proj2);
        bool match_combined2 = has_lot2 && (key_proj2 == proj_combined2);

        bool match_suffix2 = false;
        if(!match_direct2 && !match_combined2)
        {
            if((int)norm_proj2.size() <= 3 && (int)key_proj2.size() > (int)norm_proj2.size())
            {
                std::string suffix2 = key_proj2.substr(key_proj2.size() - norm_proj2.size());
                match_suffix2 = (suffix2 == norm_proj2);
            }
        }

        if(has_lot2)
        {
            if(!match_combined2 && !match_direct2 && !match_suffix2)
                continue;
        }
        else
        {
            if(!match_direct2 && !match_suffix2)
                continue;
        }

        if(key.chg != requested_chg && key.chg.find(requested_chg) != 0)
            continue;

        if(!interp(table, dist, qe, tof, drift))
            continue;

        chg = key.chg;
        return true;
    }

    chg = "";
    qe = 0;
    tof = 0;
    drift = 0;
    return false;
}

static bool solve(const std::string& proj,const std::string& lot,double dist,std::string& chg,double& qe,double& tof,double& drift)
{
    if(manual_chg_enabled)
        return solveByCharge(proj,lot,dist,manual_chg_value,chg,qe,tof,drift);

    return solveAuto(proj,lot,dist,chg,qe,tof,drift);
}

static double computeSite(double iv,double dist)
{
    if(dist==0) return 0;
    return (iv/dist)*1000.0*1.0186;
}

static int extractChargeNumber(const std::string& chg)
{
    std::string num;
    for(char c : chg)
    {
        if(std::isdigit(static_cast<unsigned char>(c)))
            num += c;
    }
    if(num.empty()) return 0;
    try { return std::stoi(num); }
    catch(...) { return 0; }
}

static double smoothstepCal(double edge0, double edge1, double x)
{
    double t = (std::max)(0.0, (std::min)(1.0, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0 - 2.0 * t);
}

static double chg6LowCurveFactor(double dist_m)
{
    const double start = 9000.0;
    const double full = 9300.0;
    const double fade_start = 9600.0;
    const double end = 10000.0;

    if(dist_m < start || dist_m >= end)
        return 0.0;

    if(dist_m < full)
        return smoothstepCal(start, full, dist_m);

    if(dist_m <= fade_start)
        return 1.0;

    return 1.0 - smoothstepCal(fade_start, end, dist_m);
}

static void hp71bCalibrate(int art_type, const std::string& proj, const std::string& lot,
                           const std::string& chg, double dist_m, double& qe, double& tof)
{
    if(art_type != 155) return;  

    const double base = 39.5;
    const double chg_scale = 13.2;
    double curve, power, qe_bias, tof_scale, tof_bias;

    int chg_num = extractChargeNumber(chg);
    std::string proj_combined = proj + lot;  

    if(proj_combined != "HEA" && proj == "HEA")
        proj_combined = "HEA";

    if(proj_combined != "HEA" && proj_combined != "HE")
    {
        if(proj.size() <= 3 && proj.size() < std::string("HEA").size())
        {
            std::string hea_suffix = std::string("HEA").substr(std::string("HEA").size() - proj.size());
            if(proj == hea_suffix)
                proj_combined = "HEA";
        }
    }

    if(proj_combined == "HEA")
    {
        if(chg_num == 6)
        {
            curve = 0.30;
            power = 1.11;
            qe_bias = -30.0;   
            tof_scale = 252.0;
            tof_bias = -1.20;
        }
        else if(chg_num == 5 || chg_num == 7)
        {
            curve = 0.32;
            power = 1.08;
            qe_bias = -5.0;
            tof_scale = 252.0;
            tof_bias = -0.30;
        }
        else
        {

            curve = 0.32;
            power = 1.08;
            qe_bias = -5.0;
            tof_scale = 252.0;
            tof_bias = -0.30;
        }
    }
    else  
    {
        curve = 0.32;
        power = 1.0;
        qe_bias = 0.0;
        tof_scale = 252.0;
        tof_bias = 0.0;
    }

    double km = dist_m / 1000.0;
    double hp71_qe = ((base * km) + (curve * km * km) + (chg_num * chg_scale)) / power + qe_bias;

    double hp71_tof = dist_m / (tof_scale * power) + chg_num * 0.16 + tof_bias;

    if(proj_combined == "HEA" && chg_num == 6)
    {
        double low_factor = chg6LowCurveFactor(dist_m);
        hp71_qe *= 1.0 - (0.075 * low_factor);
        hp71_tof *= 1.0 - (0.135 * low_factor);
    }

    qe = hp71_qe;
    tof = hp71_tof;
}

namespace Stanag4355 {

    static const int G1_N = 25;
    static const double G1_M[G1_N] = {
        0.00, 0.20, 0.40, 0.60, 0.70, 0.80, 0.85, 0.90, 0.95,
        1.00, 1.05, 1.10, 1.15, 1.20, 1.30, 1.40, 1.50, 1.60,
        1.70, 1.80, 1.90, 2.00, 2.20, 2.50, 3.00
    };
    static const double G1_Cd[G1_N] = {
        0.2300, 0.2300, 0.2300, 0.2350, 0.2500, 0.2800, 0.3100, 0.3500, 0.4000,
        0.4500, 0.4700, 0.4700, 0.4600, 0.4500, 0.4300, 0.4100, 0.3900, 0.3750,
        0.3600, 0.3500, 0.3400, 0.3300, 0.3150, 0.3000, 0.2600
    };
    static const double G1_CD0_REF = 0.2300;  

    static double g1_cd(double mach)
    {
        if(mach <= G1_M[0]) return G1_Cd[0];
        if(mach >= G1_M[G1_N-1]) return G1_Cd[G1_N-1];

        for(int i = 0; i < G1_N - 1; i++)
        {
            if(mach >= G1_M[i] && mach <= G1_M[i+1])
            {
                double t = (mach - G1_M[i]) / (G1_M[i+1] - G1_M[i]);
                return G1_Cd[i] + t * (G1_Cd[i+1] - G1_Cd[i]);
            }
        }
        return G1_Cd[G1_N-1];
    }

    struct AtmoState {
        double T;      
        double P;      
        double rho;    
        double a;      
    };

    static double cfg_temp = 28.0;     
    static double cfg_humidity = 75.0;  

    static double cfg_wind_dir = 0.0;   
    static double cfg_wind_spd = 2.5;   
    static double cfg_firing_az = 0.0;  

    static AtmoState atmosphere(double h_m)
    {

        const double T0 = 273.15 + cfg_temp;  
        const double P0 = 101325.0;           
        const double L  = 0.0065;             
        const double R  = 287.05;             
        const double g  = 9.80665;            
        const double gamma = 1.4;             

        double h = (h_m < 0) ? 0 : h_m;
        if(h > 11000.0) h = 11000.0;

        AtmoState s;
        s.T   = T0 - L * h;
        s.P   = P0 * pow(s.T / T0, g / (R * L));
        s.rho = s.P / (R * s.T);

        if(cfg_humidity > 0.0 && s.T > 273.15) {

            double es = 610.78 * exp(17.27 * (s.T - 273.15) / (s.T - 35.86));
            double pv = (cfg_humidity / 100.0) * es;  

            s.rho = s.rho * (1.0 - 0.378 * pv / s.P);
        }

        s.a   = sqrt(gamma * R * s.T);
        return s;
    }

    struct Projectile155 {
        double mass;      
        double caliber;   
        double area;      
        double v0;        
        double cd0;       
    };

    static double cfg_cd0 = 0.157;    
    static std::map<std::string, double> cfg_v0;  

    static Projectile155 default_m107()
    {
        Projectile155 p;
        p.mass    = 43.2;              
        p.caliber = 0.155;
        p.area    = 3.14159265 * pow(p.caliber / 2.0, 2);  
        p.v0      = 827.0;             
        p.cd0     = cfg_cd0;           
        return p;
    }

    static double charge_to_v0(const std::string& chg)
    {

        auto it = cfg_v0.find(chg);
        if(it != cfg_v0.end()) return it->second;

        if(chg == "3G") return 279.0;
        if(chg == "4G") return 320.0;
        if(chg == "5G") return 382.0;

        if(chg == "3W") return 295.0;
        if(chg == "4W") return 335.0;
        if(chg == "5W") return 395.0;
        if(chg == "6W") return 476.0;
        if(chg == "7W") return 574.0;

        if(chg == "7R") return 689.0;

        if(chg == "8S") return 827.0;

        int num = 0;
        for(char c : chg) { if(std::isdigit(c)) num = num * 10 + (c - '0'); }
        if(num >= 3 && num <= 7) return charge_to_v0(std::to_string(num) + "W");

        return 827.0;
    }

    static const char* charge_name(double v0)
    {
        if(v0 <= 280) return "3G";
        if(v0 <= 325) return "4G";
        if(v0 <= 385) return "5G";
        if(v0 <= 295) return "3W";
        if(v0 <= 340) return "4W";
        if(v0 <= 395) return "5W";
        if(v0 <= 478) return "6W";
        if(v0 <= 570) return "7W";
        if(v0 <= 695) return "7R";
        return "8S";
    }

    struct State {
        double x;       
        double y;       
        double vx;      
        double vy;      
    };

    static State derivatives(const State& s, const Projectile155& proj)
    {
        AtmoState atmo = atmosphere(s.y);

        double wind_vel_dir = cfg_wind_dir + 180.0;
        if(wind_vel_dir >= 360.0) wind_vel_dir -= 360.0;
        double wind_az_diff = (wind_vel_dir - cfg_firing_az) * PI / 180.0;
        double wind_x = cfg_wind_spd * cos(wind_az_diff);  

        double v_rel_x = s.vx - wind_x;
        double v_rel_y = s.vy;
        double v_rel = sqrt(v_rel_x * v_rel_x + v_rel_y * v_rel_y);

        State ds;
        ds.x = s.vx;
        ds.y = s.vy;

        if(v_rel < 0.01)
        {
            ds.vx = 0;
            ds.vy = -9.80665;
            return ds;
        }

        double mach = v_rel / atmo.a;
        double cd_g1 = g1_cd(mach);
        double cd = proj.cd0 * (cd_g1 / G1_CD0_REF);

        double Fd = 0.5 * atmo.rho * v_rel * v_rel * cd * proj.area;
        double a_drag = Fd / proj.mass;

        ds.vx = -a_drag * (v_rel_x / v_rel);
        ds.vy = -9.80665 - a_drag * (v_rel_y / v_rel);

        return ds;
    }

    static State rk4_step(const State& s, double dt, const Projectile155& proj)
    {
        State k1 = derivatives(s, proj);

        State s2;
        s2.x  = s.x  + 0.5 * dt * k1.x;
        s2.y  = s.y  + 0.5 * dt * k1.y;
        s2.vx = s.vx + 0.5 * dt * k1.vx;
        s2.vy = s.vy + 0.5 * dt * k1.vy;
        State k2 = derivatives(s2, proj);

        State s3;
        s3.x  = s.x  + 0.5 * dt * k2.x;
        s3.y  = s.y  + 0.5 * dt * k2.y;
        s3.vx = s.vx + 0.5 * dt * k2.vx;
        s3.vy = s.vy + 0.5 * dt * k2.vy;
        State k3 = derivatives(s3, proj);

        State s4;
        s4.x  = s.x  + dt * k3.x;
        s4.y  = s.y  + dt * k3.y;
        s4.vx = s.vx + dt * k3.vx;
        s4.vy = s.vy + dt * k3.vy;
        State k4 = derivatives(s4, proj);

        State result;
        result.x  = s.x  + dt * (k1.x  + 2*k2.x  + 2*k3.x  + k4.x)  / 6.0;
        result.y  = s.y  + dt * (k1.y  + 2*k2.y  + 2*k3.y  + k4.y)  / 6.0;
        result.vx = s.vx + dt * (k1.vx + 2*k2.vx + 2*k3.vx + k4.vx) / 6.0;
        result.vy = s.vy + dt * (k1.vy + 2*k2.vy + 2*k3.vy + k4.vy) / 6.0;

        return result;
    }

    static void compute_trajectory(double theta_rad, const Projectile155& proj,
                                    double& range, double& tof)
    {
        const double dt = 0.005;  

        const double g_elev = 0.0;  

        State s;
        s.x  = 0;
        s.y  = g_elev;
        s.vx = proj.v0 * cos(theta_rad);
        s.vy = proj.v0 * sin(theta_rad);

        tof = 0;
        range = 0;

        for(int i = 0; i < 200000; i++)  
        {
            State next = rk4_step(s, dt, proj);

            if(next.y < 0 && s.y >= 0)
            {

                double frac = s.y / (s.y - next.y);
                range = s.x + frac * (next.x - s.x);
                tof = tof + frac * dt;
                return;
            }

            s = next;
            tof += dt;

            if(tof > 200.0 || s.x > 25000.0)
            {
                range = s.x;
                return;
            }
        }

        range = s.x;
    }

    static double solve_qe(double target_range_m, const Projectile155& proj,
                           double& out_tof)
    {
        const double pi = 3.14159265;
        const double DEG2RAD = pi / 180.0;
        const double RAD2MILS = 6400.0 / (2.0 * pi);

        double best_range = 0, best_angle_rad = 0.35;
        double best_tof_scan = 0;
        for(int deg = 1; deg <= 59; deg += 2)
        {
            double rad = deg * DEG2RAD;
            double r, t;
            compute_trajectory(rad, proj, r, t);
            if(r > best_range)
            {
                best_range = r;
                best_angle_rad = rad;
                best_tof_scan = t;
            }
        }

        if(target_range_m >= best_range)
        {
            out_tof = best_tof_scan;
            return best_angle_rad * RAD2MILS;
        }

        double lo_rad = 0.001;  
        double hi_rad = best_angle_rad;

        for(int iter = 0; iter < 25; iter++)
        {
            double mid_rad = (lo_rad + hi_rad) / 2.0;
            double r_mid, t_mid;
            compute_trajectory(mid_rad, proj, r_mid, t_mid);

            if(r_mid < target_range_m)
                lo_rad = mid_rad;
            else
                hi_rad = mid_rad;
        }

        double final_rad = (lo_rad + hi_rad) / 2.0;
        double final_r, final_t;
        compute_trajectory(final_rad, proj, final_r, final_t);

        out_tof = final_t;
        return final_rad * RAD2MILS;
    }

} 

static std::string stanagCompare(int art_type, const std::string& proj, const std::string& lot,
                                  const std::string& chg, double dist_m)
{
    std::stringstream out;

    double qe_hp71 = 0, tof_hp71 = 0;
    {

        double qe_tmp = 0, tof_tmp = 0;
        int chg_num = 0;
        for(char c : chg) { if(std::isdigit(c)) chg_num = chg_num * 10 + (c - '0'); }

        const double base = 39.5, chg_scale = 13.2;
        double km = dist_m / 1000.0;
        double curve = 0.30, power = 1.11, qe_bias = -30.0;
        qe_hp71 = ((base * km) + (curve * km * km) + (chg_num * chg_scale)) / power + qe_bias;
        tof_hp71 = dist_m / (252.0 * power) + chg_num * 0.16 - 1.20;
    }

    double qe_stanag = 0, tof_stanag = 0;
    if(art_type == 155)
    {
        Stanag4355::Projectile155 proj_data = Stanag4355::default_m107();
        proj_data.v0 = Stanag4355::charge_to_v0(chg);  

        qe_stanag = Stanag4355::solve_qe(dist_m, proj_data, tof_stanag);
    }

    double range_km = dist_m / 1000.0;
    double spin_drift = 0.015 * range_km * range_km;  

    const double omega_e = 7.2921e-5;  
    const double lat_arg = 14.0 * 3.14159265 / 180.0;  

    double coriolis = omega_e * tof_stanag * std::sin(lat_arg) * dist_m / 100.0;

    if(dist_m > 0) coriolis = coriolis * 6400.0 / dist_m;

    out << "STANAG 4355 vs HP-71B\n";
    out << "DIST: " << (int)dist_m << "m CHG: " << chg << "\n";
    out << "HP-71B  QE=" << std::round(qe_hp71 * 10) / 10
        << " TOF=" << std::round(tof_hp71 * 100) / 100 << "\n";
    if(art_type == 155)
    {
        out << "STANAG QE=" << std::round(qe_stanag * 10) / 10
            << " TOF=" << std::round(tof_stanag * 100) / 100 << "\n";
        double diff = qe_hp71 - qe_stanag;
        out << "DIFF   QE=" << std::round(diff * 10) / 10 << " mils\n";
        out << "\nDeflection (STANAG):\n";
        out << "  Spin:  " << std::round(spin_drift * 10) / 10 << " mils R\n";
        out << "  Coriolis: " << std::round(coriolis * 10) / 10 << " mils\n";
        out << "  Total: " << std::round((spin_drift + coriolis) * 10) / 10 << " mils\n";
    }
    else
    {
        out << "STANAG: solo 155mm por ahora\n";
    }

    return out.str();
}

static std::string stanagCalibrate()
{
    std::stringstream out;
    out << "STANAG 4355 CALIBRATION v2\n";
    out << "cd0 vs FT Excel (155 HEA)\n\n";

    struct TestPt { std::string chg; double v0; double dist; double qe_ft; bool is_G; };
    std::vector<TestPt> pts;

    std::map<std::string, double> v0map = {
        {"3G",279},{"4G",320},{"5G",382},
        {"3W",292},{"4W",334},{"5W",389},{"6W",495},{"7W",565}
    };

    for(const auto& kv : firingTables)
    {
        const AmmoKey& key = kv.first;
        if(key.artillery != "155") continue;
        if(key.proj.find("HEA") == std::string::npos) continue;

        auto it = v0map.find(key.chg);
        if(it == v0map.end()) continue;

        const auto& rows = kv.second;
        if(rows.size() < 4) continue;
        double v0 = it->second;
        bool is_G = (key.chg.find("G") != std::string::npos);

        size_t n = rows.size();
        for(double pct : {0.3, 0.5, 0.7, 0.9})
        {
            size_t idx = (size_t)(n * pct);
            if(idx >= n) idx = n - 1;
            pts.push_back({key.chg, v0, rows[idx].d, rows[idx].qe, is_G});
        }
    }

    int n_G_total = 0, n_W_total = 0;
    for(const auto& p : pts) { if(p.is_G) n_G_total++; else n_W_total++; }
    out << "Test points: " << pts.size() << " (G:" << n_G_total << " W:" << n_W_total << ")\n\n";

    const int N_CD0 = 15;
    const int N_PTS = (int)pts.size();
    double cd0_vals[N_CD0];
    double cached_err[N_CD0][96]; 
    int n_pts = N_PTS;
    if(n_pts > 96) n_pts = 96;

    out << "  cd0    RMSE   MAXERR   ABMEAN  RMSE_G  RMSE_W\n";
    out << "------ ------ ------ -------- ------- -------\n";

    double best_rmse_cd0 = 0.165, best_rmse_val = 1e9;
    double best_mm_cd0 = 0.165, best_mm_val = 1e9;

    int ci = 0;
    for(int cd0_x1000 = 130; cd0_x1000 <= 200; cd0_x1000 += 5, ci++)
    {
        double cd0 = cd0_x1000 / 1000.0;
        cd0_vals[ci] = cd0;
        double sum_sq = 0, max_err = 0, sum_abs = 0;
        double sum_sq_G = 0, sum_sq_W = 0;
        int n_G = 0, n_W = 0;

        for(int pi = 0; pi < n_pts; pi++)
        {
            const auto& pt = pts[pi];
            Stanag4355::Projectile155 proj = Stanag4355::default_m107();
            proj.v0 = pt.v0;
            proj.cd0 = cd0;

            double tof_unused = 0;
            double qe_stanag = Stanag4355::solve_qe(pt.dist, proj, tof_unused);
            double err = qe_stanag - pt.qe_ft;
            cached_err[ci][pi] = err;

            sum_sq += err * err;
            sum_abs += std::abs(err);
            if(std::abs(err) > max_err) max_err = std::abs(err);

            if(pt.is_G) { sum_sq_G += err * err; n_G++; }
            else        { sum_sq_W += err * err; n_W++; }
        }

        int n_total = n_G + n_W;
        double rmse = std::sqrt(sum_sq / n_total);
        double rmse_G = n_G > 0 ? std::sqrt(sum_sq_G / n_G) : 0;
        double rmse_W = n_W > 0 ? std::sqrt(sum_sq_W / n_W) : 0;

        if(rmse < best_rmse_val) { best_rmse_val = rmse; best_rmse_cd0 = cd0; }
        if(max_err < best_mm_val) { best_mm_val = max_err; best_mm_cd0 = cd0; }

        char buf[128];
        sprintf(buf, "%6.3f %6.1f %6.1f %+7.1f %7.1f %7.1f",
                cd0, rmse, max_err, sum_abs/n_total, rmse_G, rmse_W);
        out << buf << "\n";
    }

    int best_gi = 0, best_wi = 0;
    double best_split_rmse = best_rmse_val;
    for(int gi = 0; gi < ci; gi++)
    {
        for(int wi = 0; wi < ci; wi++)
        {
            double ss = 0;
            for(int pi = 0; pi < n_pts; pi++)
            {
                double err = pts[pi].is_G ? cached_err[gi][pi] : cached_err[wi][pi];
                ss += err * err;
            }
            double rmse_split = std::sqrt(ss / n_pts);
            if(rmse_split < best_split_rmse) {
                best_split_rmse = rmse_split;
                best_gi = gi;
                best_wi = wi;
            }
        }
    }

    double best_G = cd0_vals[best_gi], best_W = cd0_vals[best_wi];
    for(int delta = -1; delta <= 1; delta++)
    {
        double cd0g = cd0_vals[best_gi] + delta * 0.001;
        if(cd0g < 0.130 || cd0g > 0.200) continue;
        for(int delta2 = -1; delta2 <= 1; delta2++)
        {
            double cd0w = cd0_vals[best_wi] + delta2 * 0.001;
            if(cd0w < 0.130 || cd0w > 0.200) continue;

            double ss = 0;
            for(int pi = 0; pi < n_pts; pi++)
            {
                Stanag4355::Projectile155 proj = Stanag4355::default_m107();
                proj.v0 = pts[pi].v0;
                proj.cd0 = pts[pi].is_G ? cd0g : cd0w;
                double tof_u = 0;
                double qe_s = Stanag4355::solve_qe(pts[pi].dist, proj, tof_u);
                double err = qe_s - pts[pi].qe_ft;
                ss += err * err;
            }
            double rmse_split = std::sqrt(ss / n_pts);
            if(rmse_split < best_split_rmse) {
                best_split_rmse = rmse_split;
                best_G = cd0g;
                best_W = cd0w;
            }
        }
    }

    out << "\n=== OPTIMAL cd0 ===\n";
    out << "  RMSE:       " << std::fixed << std::setprecision(3) << best_rmse_cd0
        << " (RMSE=" << std::setprecision(1) << best_rmse_val << ")\n";
    out << "  MINIMAX:    " << best_mm_cd0
        << " (maxerr=" << best_mm_val << ")\n";
    out << "  SPLIT G/W:  G=" << best_G << " W=" << best_W
        << " (RMSE=" << best_split_rmse << ")\n";

    return out.str();
}

static bool parseUD(const std::string& cmd,double& value_out)
{
    if(cmd.empty()) return false;

    if(cmd.size() >= 2 && (cmd[0]=='U' || cmd[0]=='D'))
    {
        try
        {
            double v = std::stod(cmd.substr(1));
            value_out = (cmd[0]=='U') ? v : -v;
            return true;
        }
        catch(...)
        {

        }
    }

    std::stringstream ss(cmd);
    std::string dir;
    double val = 0.0;
    ss >> dir >> val;

    if(dir=="U")
    {
        value_out = val;
        return true;
    }

    if(dir=="D")
    {
        value_out = -val;
        return true;
    }

    return false;
}

    std::string getCurrentTime()
    {
        std::time_t now = std::time(nullptr);
        std::tm* local = std::localtime(&now);

        std::stringstream ss;
        ss << std::put_time(local, "%Y-%m-%d %H:%M:%S");

        return ss.str();
    }

std::string BasicEngine::execute(const std::string& input)
{
    std::string cmd = input;

    if(boot_mode)
    {
        std::string v = normStr(cmd);

        if(v.empty())
        {
            return "SANTA BARBARA\n>";
        }

        if(v == "RUNBUCS")
        {
            boot_mode = false;
            initializing = true;

            return "INITIALIZE";
        }

        return "SANTA BARBARA\n>";
    }

    if(initializing)
    {
        initializing = false;
        current_menu = "MAIN";

        return "DR EVIL\nMAIN (? 1 3 4 5 7 X *)";
    }

    auto renderFire = [&](bool useFFE)->std::string
{
    std::stringstream out;

    double base_e = guns[base_piece_index].e;
    double base_n = guns[base_piece_index].n;
    double base_alt = guns[base_piece_index].alt;

    double adj_tgt_e = tgt_e;
    double adj_tgt_n = tgt_n;

    if(shift_lr != 0 || shift_ad != 0)
    {
        double dir_rad = shift_angle * (2.0 * PI / 6400.0);

        double ad_e = std::sin(dir_rad);
        double ad_n = std::cos(dir_rad);

        double lr_e = std::cos(dir_rad);
        double lr_n = -std::sin(dir_rad);

        double delta_e = (ad_e * shift_ad) + (lr_e * shift_lr);
        double delta_n = (ad_n * shift_ad) + (lr_n * shift_lr);

        adj_tgt_e += delta_e;
        adj_tgt_n += delta_n;
    }

    for(size_t i=0;i<guns.size();i++)
    {
        double dx = adj_tgt_e - guns[i].e;
        double dy = adj_tgt_n - guns[i].n;

        double dist_geom = std::sqrt(dx*dx + dy*dy);

        double az = std::atan2(dx,dy);
        if(az < 0) az += 2*PI;

        double mils_raw = az * (6400 / (2 * PI));

        double mils = std::floor((mils_raw + 1.5) / 2.0) * 2.0;

        std::string chg = "";
        double qe = 0, tof = 0, drift = 0;

        double dist = dist_geom + reg_dist;

        bool solved = solve(ammo_proj_prop, ammo_proj_lot, dist, chg, qe, tof, drift);

        hp71bCalibrate(std::stoi(artillery_type), ammo_proj_prop, ammo_proj_lot, chg, dist, qe, tof);

        if(std::abs(drift) < 0.01)
        {

            if(artillery_type == "155")
                drift = 0.000516 * dist;
            else
                drift = 0.0001 * dist;
        }

        bool use_fm1_reverse = fm1_base_def_reverse || active_fm1_def_reverse;

        double def = 0.0;

        def = def_base - (mils - az_lay);

        if(!use_fm1_reverse)
            def += reg_def + df_corr;

        double drift_accum = drift;  
        double jump_h = 6.6;

        if(use_fm1_reverse)
        {
            drift_accum = 0.0;
            jump_h = 0.0;
        }

        if(manual_chg_enabled)
            chg = manual_chg_value;

        double iv = tgt_alt - guns[i].alt + shift_ud;
        double site = computeSite(iv,dist);

        double qe_final = qe + site + time_reg_correction;
        if(ud_active)
            qe_final += (ud_corr*0.05);

        if(sheaf_mode == "OPEN" && guns.size() > 1)
        {
            int total_guns = (int)guns.size();

            double center = (total_guns - 1) / 2.0;
            double spacing = sheaf_width / (total_guns - 1);

            double offset = (i - center) * spacing;

            def += offset;
        }

        def += drift_accum;
        def += jump_h;

        def = std::round(def);

        while(def < 0) def += 6400;
        while(def >= 6400) def -= 6400;

        if(!solved)
        {
            out<<"----- PIECE "<<i+1<<" -----\n";
            out<<(useFFE ? "FFE\n" : "MISSION\n");
            out<<"DIST "<<std::round(dist)<<"\n";
            out<<"AZ "<<std::round(mils)<<"\n";
            out<<"DEF NO DATA\n";
            out<<"CHG NO DATA\n";
            out<<"QE NO DATA\n";
            out<<"TOF NO DATA\n";

            if(fuze_time_mode)
            {
                if(hob > 0)
                    out<<"FUZE TIA HOB "<<std::round(hob)<<"\n\n";
                else
                    out<<"FUZE TIA\n\n";
            }
            else
            {
                out<<"FUZE PDA\n\n";
            }

            continue;
        }

        out<<"----- PIECE "<<i+1<<" -----\n";
        out<<(useFFE ? "FFE\n" : "MISSION\n");
        out<<"DIST "<<std::round(dist)<<"\n";
        out<<"AZ "<<std::round(mils)<<"\n";
        std::string def_output;

        while(def < 0) def += 6400;
        while(def >= 6400) def -= 6400;

        int def_corrected = (int)std::round(def);

        def_output = std::to_string(def_corrected);
        out<<"DEF "<<def_output<<"\n";
        out<<"CHG "<<chg<<"\n";
        out<<"QE "<<std::round(qe_final*10)/10<<"\n";
        out<<"TOF "<<tof<<"\n";

        if(fuze_time_mode)
        {
            if(hob > 0)
                out<<"FUZE TIA HOB "<<std::round(hob)<<"\n\n";
            else
                out<<"FUZE TIA\n\n";
        }
        else
        {
            out<<"FUZE PDA\n\n";
        }
    }

        int i = base_piece_index;

        double dx = adj_tgt_e - guns[i].e;
        double dy = adj_tgt_n - guns[i].n;

        double dist_geom = std::sqrt(dx*dx + dy*dy);

        double az = std::atan2(dx,dy);
        if(az < 0) az += 2*PI;

        double mils_raw = az * (6400 / (2 * PI));
        double mils = std::floor((mils_raw + 1.5) / 2.0) * 2.0;

        double dist = dist_geom + reg_dist;

        std::string chg_tmp = "";
        double qe_tmp = 0, tof_tmp = 0, drift_tmp = 0;

        bool solved_ref = solve(ammo_proj_prop, ammo_proj_lot, dist, chg_tmp, qe_tmp, tof_tmp, drift_tmp);

        if(solved_ref)
            hp71bCalibrate(std::stoi(artillery_type), ammo_proj_prop, ammo_proj_lot, chg_tmp, dist, qe_tmp, tof_tmp);

        if(std::abs(drift_tmp) < 0.001)
        {
            drift_tmp = 0.00038 * dist;
        }

        if(solved_ref)
        {
        bool use_fm1_reverse_ref = fm1_base_def_reverse || active_fm1_def_reverse;

        double def_ref = 0.0;

        def_ref = def_base - (mils - az_lay);

        if(!use_fm1_reverse_ref)
            def_ref += reg_def + df_corr;

            double drift_accum = drift_tmp;  
            double jump_h = 6.6;

        if(use_fm1_reverse_ref)
        {
            drift_accum = 0.0;
            jump_h = 0.0;
        }

            def_ref += drift_accum;
            def_ref += jump_h;

            def_ref = std::round(def_ref);

            while(def_ref < 0) def_ref += 6400;
            while(def_ref >= 6400) def_ref -= 6400;

            double iv_ref = tgt_alt - guns[i].alt + shift_ud;
            double site_ref = computeSite(iv_ref, dist);

            double qe_final_ref = qe_tmp + site_ref + time_reg_correction;

            if(ud_active)
                qe_final_ref += (ud_corr * 0.05);

            while(def_ref < 0) def_ref += 6400;
            while(def_ref >= 6400) def_ref -= 6400;

            last_def_solution = def_ref;
            last_dist_solution = dist;
            last_qe_solution = qe_final_ref;
        }
    return out.str();
};

auto computeAngleTFromDir = [&](double dir_mils) -> double
{
    if(guns.empty())
        return 0.0;

    if(base_piece_index < 0 || base_piece_index >= (int)guns.size())
        return 0.0;

    double dx = tgt_e - guns[base_piece_index].e;
    double dy = tgt_n - guns[base_piece_index].n;

    double az = std::atan2(dx, dy);

    if(az < 0)
        az += 2.0 * PI;

    double gun_target_mils = az * (6400.0 / (2.0 * PI));

    gun_target_mils = std::floor((gun_target_mils + 1.5) / 2.0) * 2.0;

    double angle_t = std::fabs(dir_mils - gun_target_mils);

    if(angle_t > 3200.0)
        angle_t = 6400.0 - angle_t;

    return std::round(angle_t);
};

auto computeBaseAz = [&]() -> double
{
    if(guns.empty())
        return 0.0;

    if(base_piece_index < 0 || base_piece_index >= (int)guns.size())
        return 0.0;

    double dx = tgt_e - guns[base_piece_index].e;
    double dy = tgt_n - guns[base_piece_index].n;

    double az = std::atan2(dx, dy);

    if(az < 0)
        az += 2.0 * PI;

    double az_mils = az * (6400.0 / (2.0 * PI));

    az_mils = std::floor((az_mils + 1.5) / 2.0) * 2.0;

    while(az_mils < 0)
        az_mils += 6400.0;

    while(az_mils >= 6400.0)
        az_mils -= 6400.0;

    return std::round(az_mils);
};

auto applyObserverCorrection = [&](double obs_lr, double obs_ad, double obs_ud, double dir_mils)
{

    while(dir_mils < 0.0)
        dir_mils += 6400.0;

    while(dir_mils >= 6400.0)
        dir_mils -= 6400.0;

    double angle_t = computeAngleTFromDir(dir_mils);
    double t_rad = angle_t * (2.0 * PI / 6400.0);

    double real_lr = (obs_lr * std::cos(t_rad)) - (obs_ad * std::sin(t_rad));
    double real_ad = (obs_ad * std::cos(t_rad)) - (obs_lr * std::sin(t_rad));

    shift_angle = computeBaseAz();

    shift_lr += real_lr;
    shift_ad += real_ad;
    shift_ud += obs_ud;

    ud_active = false;
    ud_corr = 0.0;
};

auto prepareTgtBaseShotForObserverCorrections = [&]()
{

    fire_phase = 0;

    shift_lr = 0.0;
    shift_ad = 0.0;
    shift_ud = 0.0;

    ud_active = false;
    ud_corr = 0.0;

    inst_prev_dir = 0.0;
    inst_prev_lr = 0.0;
    inst_prev_ad = 0.0;
    inst_prev_ud = 0.0;

    inst_new_dir = 0.0;
    inst_angle_t = 0.0;
    inst_last_dir = 0.0;

    inst_lr_shift = 0.0;
    inst_ad_shift = 0.0;
    inst_ud_shift = 0.0;

    all_guns_command = false;
    ffe_mode = false;

    chg_allowed = true;
};

auto finishTgtBaseShot = [&]() -> std::string
{
    prepareTgtBaseShotForObserverCorrections();

    current_menu = "FM1_BASE_PIECE";

    return "BASE PIECE (P *): " + std::to_string(base_piece_index + 1);
};

std::string v = normStr(cmd);

if(v == "E")
{
    std::stringstream out;
    out << "TESON EVIL\n";
    out << "EOM - END OF MISSION\n";

    if(!mission_log.empty())
        out << mission_log.back().label << "\n"
            << mission_log.back().inputs
            << mission_log.back().fire;

    out << "\nMISSION CLOSED\n";

    mission_counter++;

    mission_active=false;
    ffe_mode=false;

    return out.str() + "MAIN (? 1 3 4 5 7 X *)";
}

    if(cmd=="*")
    {
        current_menu="MAIN";
        return drPrefix("MAIN",mainMenu());
    }

    if(current_menu=="MAIN")
    {
        if(cmd=="1")
        {
            current_menu="FM";
            mission_active=true;
            return drPrefix("FM",fireMenu());
        }

        if(cmd=="3")
        {
            current_menu = "TARGET";
            input_stage = 0;
            return "TGT:";
        }

        if(cmd=="4")
        {
            current_menu = "OBS";
            input_stage = 0;

            if(obs_current_index <= 0)
                obs_current_index = 1;

            BasicEngine::obs_id = obs_current_index;

            return "PO #" + std::to_string(obs_current_index) + " (P *): " + std::to_string(obs_current_index);
        }

        if(cmd=="5")
        {
            current_menu="AFU";
            return afuMenu();
        }

        if(cmd=="7")
        {
            current_menu="MAP_MODEL";
            input_stage=0;

            return "MAX E (P *): " + std::to_string((int)map_e_max);
        }

        if(cmd=="X")
            return resetData();

        return drPrefix("MAIN",mainMenu());
    }

if(current_menu=="TARGET")
{
    static int temp_knpt = 0;

    if(cmd=="P")
    {
        if(input_stage > 0)
            input_stage--;

        switch(input_stage)
        {
            case 0:
                return "TGT (P *): " + current_tgt_indicator;

            case 1:
                return "TGT/KNPT # (P *): " + std::to_string(temp_knpt);

            case 2:
                return "TGT E (P *): " + std::to_string((int)tgt_e);

            case 3:
                return "TGT N (P *): " + std::to_string((int)tgt_n);

            case 4:
                return "TGT ALT (P *): " + std::to_string((int)tgt_alt);
        }
    }

    if(cmd=="*")
    {
        current_menu = "MAIN";
        input_stage = 0;
        return "MAIN (? 1 3 4 5 7 X *)";
    }

    std::string v = normStr(cmd);

    if(input_stage==0)
    {
        if(!v.empty())
            current_tgt_indicator = v;

        input_stage++;
        return "TGT/KNPT # (P *): " + std::to_string(temp_knpt);
    }

    if(input_stage==1)
    {
        if(!v.empty())
            temp_knpt = std::stoi(v);

        input_stage++;
        return "TGT E (P *): " + std::to_string((int)tgt_e);
    }

    if(input_stage==2)
    {
        if(!v.empty())
            tgt_e = std::stod(v);

        input_stage++;
        return "TGT N (P *): " + std::to_string((int)tgt_n);
    }

    if(input_stage==3)
    {
        if(!v.empty())
            tgt_n = std::stod(v);

        input_stage++;
        return "TGT ALT (P *): " + std::to_string((int)tgt_alt);
    }

    if(input_stage==4)
    {
        if(!v.empty())
            tgt_alt = std::stod(v);

        targets[temp_knpt] = {tgt_e, tgt_n, tgt_alt};
        current_knpt = temp_knpt;

        current_menu = "MAIN";
        input_stage = 0;

        main_inputs.push_back("TARGET");
        main_inputs.push_back("TGT " + current_tgt_indicator);
        main_inputs.push_back("KNPT " + std::to_string(temp_knpt));
        main_inputs.push_back("ALT " + std::to_string((int)tgt_alt));
        main_inputs.push_back("N " + std::to_string((int)tgt_n));
        main_inputs.push_back("E " + std::to_string((int)tgt_e));

        return "TGT " + current_tgt_indicator + " / KNPT " + std::to_string(temp_knpt) + " STORED\nMAIN (? 1 3 4 5 7 X *)";
    }
}

    if(current_menu=="AFU")
    {
        if(cmd=="1")
        {
             current_menu="ART_TYPE";
            return "ART (105/155):";
        }

        if(cmd=="3")   
        {
            current_menu="MET";
            input_stage=0;
            return "DIR:";
        }

        if(cmd=="5")
        {
            current_menu="AMMO";
            input_stage=0;
            return ammoMenu();
        }

        return afuMenu();
    }

    if(current_menu=="ART_TYPE")
    {
        if(cmd=="105" || cmd=="155")
        {
            artillery_type = cmd;
            current_menu="COB_QTY";
            return "QTY PIECE:";
        }

        return "ART (105/155):";
    }

    if(current_menu=="AMMO")
    {
        if(cmd=="I")
        {
            input_stage=0;
            ammo_input_active=true;
            return "PROJ:";
        }

        if(!ammo_input_active)
            return ammoMenu();

        if(input_stage==0)
        {
            ammo_proj_prop=cmd;
            input_stage++;
            return "LOT:";
        }

        if(input_stage==1)
        {
            ammo_proj_lot=cmd;

            main_inputs.push_back("AMMO");
            main_inputs.push_back("PROJ " + ammo_proj_prop);
            main_inputs.push_back("LOT " + ammo_proj_lot);

            ammo_input_active=false;
            input_stage=0;
            current_menu="AFU";

            return "AMMO STORED\nAFU INDEX (? 1 3 5 *)";
        }

        if(input_stage==2)
        {
            ammo_proj_wt=std::stod(cmd);
            ammo_input_active=false;
            input_stage=0;
            current_menu="AFU";
            return "AMMO STORED\nAFU INDEX (? 1 3 5 *)";
        }
    }

    if(current_menu=="COB_QTY")
    {
        if(cmd=="P")
            return "QTY PIECE (P *): " + std::to_string(cob_expected_qty);

        std::string v = normStr(cmd);

        guns.clear();

        if(!v.empty())
            cob_expected_qty = std::stoi(v);

        cob_current_index = 0;

        current_menu = "COB_BASE";
        return "BASE PIECE (P *): " + std::to_string(base_piece_index + 1);
    }

    if(current_menu=="COB_BASE")
    {
        if(cmd=="P")
        {
            current_menu = "COB_QTY";
            return "QTY PIECE (P *): " + std::to_string(cob_expected_qty);
        }

        std::string v = normStr(cmd);

        int bp = base_piece_index + 1;

        if(!v.empty())
            bp = std::stoi(v);

        if(bp <= 0)
            return "INVALID BASE";

        if(bp > cob_expected_qty)
            return "BASE > QTY";

        base_piece_index = bp - 1;

        current_menu = "COB_GB_E";
        return "GB E (P *): " + std::to_string((int)gb_e);
    }
    if(current_menu=="COB_GB_E")
    {
        if(cmd=="P")
        {
            current_menu = "COB_BASE";
            return "BASE PIECE (P *): " + std::to_string(base_piece_index + 1);
        }

        std::string v = normStr(cmd);

        if(!v.empty())
            gb_e = std::stod(v);

        current_menu = "COB_GB_N";
        return "GB N (P *): " + std::to_string((int)gb_n);
    }

    if(current_menu=="COB_GB_N")
    {
        if(cmd=="P")
        {
            current_menu = "COB_GB_E";
            return "GB E (P *): " + std::to_string((int)gb_e);
        }

        std::string v = normStr(cmd);

        if(!v.empty())
            gb_n = std::stod(v);

        current_menu = "COB_GB_ALT";
        return "GB ALT (P *): " + std::to_string((int)gb_alt);
    }

    if(current_menu=="COB_GB_ALT")
    {
        if(cmd=="P")
        {
            current_menu = "COB_GB_N";
            return "GB N (P *): " + std::to_string((int)gb_n);
        }

        std::string v = normStr(cmd);

        if(!v.empty())
            gb_alt = std::stod(v);

        cob_current_index = 1;

        if(cob_expected_qty <= 0)
        {
            current_menu="MAIN";
            return "COB STORED\nMAIN (? 1 3 4 5 7 X *)";
        }

        current_menu = "COB_AZ_LAY";
        return "AZ LAY (P *): " + std::to_string((int)az_lay);
    }

    if(current_menu=="COB_AZ_LAY")
    {
        if(cmd=="P")
        {
            current_menu = "COB_GB_ALT";
            return "GB ALT (P *): " + std::to_string((int)gb_alt);
        }

        std::string v = normStr(cmd);

        if(!v.empty())
            az_lay = std::stod(v);

        current_menu = "COB_REF_DEF";
        return "REF DEF (P *): " + std::to_string(def_base);
    }

    if(current_menu=="COB_REF_DEF")
    {
        if(cmd=="P")
        {
            current_menu = "COB_AZ_LAY";
            return "AZ LAY (P *): " + std::to_string((int)az_lay);
        }

        std::string v = normStr(cmd);

        if(!v.empty())
            def_base = std::stoi(v);

        main_inputs.push_back("COB");
        main_inputs.push_back("GB E " + std::to_string((int)gb_e));
        main_inputs.push_back("GB N " + std::to_string((int)gb_n));
        main_inputs.push_back("ALT " + std::to_string((int)gb_alt));
        main_inputs.push_back("AZ " + std::to_string((int)az_lay));
        main_inputs.push_back("REF DEF " + std::to_string(def_base));
        main_inputs.push_back("BASE PIECE " + std::to_string(base_piece_index + 1));

        cob_current_index = 1;

        if(cob_expected_qty <= 0)
        {
            current_menu="MAIN";
            return "COB STORED\nMAIN (? 1 3 4 5 7 X *)";
        }

        current_menu = "COB_DIR";

        std::stringstream ss;
        ss << "#" << cob_current_index << " DIR (P *): " << (int)temp_dir;
        return ss.str();
    }

    if(current_menu=="COB_DIR")
    {
        if(cmd=="P")
        {
            if(cob_current_index > 1)
            {
                cob_current_index--;
                current_menu = "COB_IV";

                std::stringstream ss;
                ss << "#" << cob_current_index << " IV (P *): " << (int)temp_iv;
                return ss.str();
            }

            current_menu = "COB_REF_DEF";
            return "REF DEF (P *): " + std::to_string(def_base);
        }

        std::string v = normStr(cmd);

        if(!v.empty())
            temp_dir = std::stod(v);

        current_menu = "COB_DIST";

        std::stringstream ss;
        ss << "#" << cob_current_index << " DIST (P *): " << (int)temp_dist;
        return ss.str();
    }

    if(current_menu=="COB_DIST")
    {
        if(cmd=="P")
        {
            current_menu = "COB_DIR";

            std::stringstream ss;
            ss << "#" << cob_current_index << " DIR (P *): " << (int)temp_dir;
            return ss.str();
        }

        std::string v = normStr(cmd);

        if(!v.empty())
            temp_dist = std::stod(v);

        current_menu = "COB_IV";

        std::stringstream ss;
        ss << "#" << cob_current_index << " IV (P *): " << (int)temp_iv;
        return ss.str();
    }

    if(current_menu=="COB_IV")
    {
        if(cmd=="P")
        {
            current_menu = "COB_DIST";

            std::stringstream ss;
            ss << "#" << cob_current_index << " DIST (P *): " << (int)temp_dist;
            return ss.str();
        }

        std::string v = normStr(cmd);

        if(!v.empty())
            temp_iv = std::stod(v);

        if(temp_dist < 0)
        return "DIST INVALID";

        if(temp_dir < 0 || temp_dir >= 6400)
            return "DIR INVALID";

        double az = temp_dir * (2 * PI / 6400.0);

        double e = gb_e + std::sin(az) * temp_dist;
        double n = gb_n + std::cos(az) * temp_dist;
        double alt = gb_alt + temp_iv;

        Gun g;
        g.e = e;
        g.n = n;
        g.alt = alt;
        g.dir = temp_dir;

        guns.push_back(g);

        cob_current_index++;

        temp_dir = 0;
        temp_dist = 0;
        temp_iv = 0;

        if(cob_current_index > cob_expected_qty)
        {
            current_menu="MAIN";
            return "COB STORED\nMAIN (? 1 3 4 5 7 X *)";
        }

        current_menu = "COB_DIR";

        std::stringstream ss;
        ss << "#" << cob_current_index << " DIR (P *): " << (int)temp_dir;
        return ss.str();
    }

if(current_menu=="OBS")
{
    if(cmd=="*")
    {
        current_menu = "MAIN";
        input_stage = 0;
        return "MAIN (? 1 3 4 5 7 X *)";
    }

    if(cmd=="P")
    {
        if(input_stage > 0)
            input_stage--;

        switch(input_stage)
        {
            case 0:
                return "PO #" + std::to_string(obs_current_index) + " (P *): " + std::to_string(obs_current_index);

            case 1:
                return "PO #" + std::to_string((int)BasicEngine::obs_id) + " E (P *): " + std::to_string((int)obs_e);

            case 2:
                return "PO #" + std::to_string((int)BasicEngine::obs_id) + " N (P *): " + std::to_string((int)obs_n);

            case 3:
                return "PO #" + std::to_string((int)BasicEngine::obs_id) + " ALT (P *): " + std::to_string((int)obs_alt);
        }
    }

    std::string v = normStr(cmd);

    if(input_stage == 0)
    {
        int selected_po = obs_current_index;

        if(!v.empty())
        {
            try
            {
                selected_po = std::stoi(v);
            }
            catch(...)
            {
                return "PO #" + std::to_string(obs_current_index) + " (P *): " + std::to_string(obs_current_index);
            }
        }

        if(selected_po <= 0)
            selected_po = obs_current_index;

        BasicEngine::obs_id = selected_po;

        if(observers.find(BasicEngine::obs_id) != observers.end())
        {
            obs_e = observers[BasicEngine::obs_id].e;
            obs_n = observers[BasicEngine::obs_id].n;
            obs_alt = observers[BasicEngine::obs_id].alt;
            BasicEngine::obs_gz = observers[BasicEngine::obs_id].gz;
        }
        else
        {
            obs_e = 0;
            obs_n = 0;
            obs_alt = 0;
            BasicEngine::obs_gz = 0;
        }

        input_stage = 1;

        return "PO #" + std::to_string((int)BasicEngine::obs_id) + " E (P *): " + std::to_string((int)obs_e);
    }

    if(input_stage == 1)
    {
        if(!v.empty())
        {
            try
            {
                obs_e = std::stod(v);
            }
            catch(...)
            {
                return "PO #" + std::to_string((int)BasicEngine::obs_id) + " E (P *): " + std::to_string((int)obs_e);
            }
        }

        input_stage = 2;

        return "PO #" + std::to_string((int)BasicEngine::obs_id) + " N (P *): " + std::to_string((int)obs_n);
    }

    if(input_stage == 2)
    {
        if(!v.empty())
        {
            try
            {
                obs_n = std::stod(v);
            }
            catch(...)
            {
                return "PO #" + std::to_string((int)BasicEngine::obs_id) + " N (P *): " + std::to_string((int)obs_n);
            }
        }

        input_stage = 3;

        return "PO #" + std::to_string((int)BasicEngine::obs_id) + " ALT (P *): " + std::to_string((int)obs_alt);
    }

    if(input_stage == 3)
    {
        if(!v.empty())
        {
            try
            {
                obs_alt = std::stod(v);
            }
            catch(...)
            {
                return "PO #" + std::to_string((int)BasicEngine::obs_id) + " ALT (P *): " + std::to_string((int)obs_alt);
            }
        }

        ObserverData obs;
        obs.id = BasicEngine::obs_id;
        obs.e = obs_e;
        obs.n = obs_n;
        obs.alt = obs_alt;

        obs.gz = BasicEngine::obs_gz;

        observers[obs.id] = obs;

        main_inputs.push_back("PO " + std::to_string(obs.id));
        main_inputs.push_back("E " + std::to_string((int)obs.e));
        main_inputs.push_back("N " + std::to_string((int)obs.n));
        main_inputs.push_back("ALT " + std::to_string((int)obs.alt));
        main_inputs.push_back("GZ " + std::to_string((int)obs.gz));

        if(obs.id >= obs_current_index)
            obs_current_index = obs.id + 1;

        BasicEngine::obs_id = obs_current_index;

        current_menu = "MAIN";
        input_stage = 0;

        return "PO " + std::to_string(obs.id) + " STORED\nMAIN (? 1 3 4 5 7 X *)";
    }
}

if(current_menu=="MAP_MODEL")
{

    if(cmd=="P")
    {
        if(input_stage > 0) input_stage--;

        switch(input_stage)
        {
            case 0: return "MAX E (P *): " + std::to_string((int)map_e_max);
            case 1: return "MIN E (P *): " + std::to_string((int)map_e_min);
            case 2: return "MAX N (P *): " + std::to_string((int)map_n_max);
            case 3: return "MIN N (P *): " + std::to_string((int)map_n_min);
            case 4: return "GZ (P *): " + std::to_string((int)map_gz);
            case 5: return "SPHER (P *): " + map_spher;
        }
    }

    std::string v = normStr(cmd);

    if(!v.empty())
    {
        switch(input_stage)
        {
            case 0: map_e_max = std::stod(v); break;
            case 1: map_e_min = std::stod(v); break;
            case 2: map_n_max = std::stod(v); break;
            case 3: map_n_min = std::stod(v); break;
            case 4: map_gz    = std::stod(v); break;
            case 5: map_spher = v; break;
            std::cout << "MAP SAVED: " << map_e_max << std::endl;
        }
    }

    input_stage++;

    switch(input_stage)
    {
        case 1: return "MIN E (P *): " + std::to_string((int)map_e_min);
        case 2: return "MAX N (P *): " + std::to_string((int)map_n_max);
        case 3: return "MIN N (P *): " + std::to_string((int)map_n_min);
        case 4: return "GZ (P *): " + std::to_string((int)map_gz);
        case 5: return "SPHER (P *): " + map_spher;
        case 6:
        {
            main_inputs.push_back("MAP MODEL");
            main_inputs.push_back("EMAX " + std::to_string((int)map_e_max));
            main_inputs.push_back("EMIN " + std::to_string((int)map_e_min));
            main_inputs.push_back("NMAX " + std::to_string((int)map_n_max));
            main_inputs.push_back("NMIN " + std::to_string((int)map_n_min));
            main_inputs.push_back("GZ " + std::to_string((int)map_gz));
            main_inputs.push_back("SPHER " + map_spher);

            double center_e = (map_e_max + map_e_min) / 2.0;
            double center_n = (map_n_max + map_n_min) / 2.0;

            struct ZoneAtm {
                const char* name;
                double e, n;            
                double temp, hum;       
                double wind_dir, wind_spd; 
            };

            ZoneAtm zones[] = {
                {"ZAMBRANO",   456854, 1577256, 32.0, 80.0,   0, 3.4},
                {"PINALEJO",   383483, 1649393, 25.0, 85.0,   0, 3.0},
                {"TRINCHERAS", 479843, 1470565, 34.0, 65.0, 180, 3.0},
            };

            const int NUM_ZONES = 3;
            double min_dist = 1e9;
            int best_idx = -1;

            for(int i = 0; i < NUM_ZONES; i++)
            {
                double dx = center_e - zones[i].e;
                double dy = center_n - zones[i].n;
                double dist = sqrt(dx*dx + dy*dy);
                if(dist < min_dist)
                {
                    min_dist = dist;
                    best_idx = i;
                }
            }

            std::string zone_name = "UNKNOWN";
            if(best_idx >= 0 && min_dist < 50000) 
            {
                zone_name = zones[best_idx].name;
                temperature = zones[best_idx].temp;
                Stanag4355::cfg_temp = zones[best_idx].temp;
                Stanag4355::cfg_humidity = zones[best_idx].hum;
                wind_dir = zones[best_idx].wind_dir;
                wind_speed = zones[best_idx].wind_spd;
                Stanag4355::cfg_wind_dir = zones[best_idx].wind_dir;
                Stanag4355::cfg_wind_spd = zones[best_idx].wind_spd;
            }

            current_menu="MAIN";
            input_stage=0;

            return "MAP STORED\nMAIN (? 1 3 4 5 7 X *)";
        }
    }

    return "MAX E (P *): " + std::to_string((int)map_e_max);
}

    if(current_menu=="MET")
    {
        switch(input_stage)
        {
            case 0:
                wind_dir = std::stod(cmd);
                input_stage++;
                return "VEL:";

            case 1:
                wind_speed = std::stod(cmd);
                input_stage++;
                return "TEMP:";

            case 2:
                temperature = std::stod(cmd);
                current_menu="AFU";
                input_stage=0;

                return "MET STORED\nAFU INDEX (? 1 3 5 *)";
        }
    }

    if(current_menu=="FM")
    {

        if(cmd=="AUTOCHG")
        {
            manual_chg_enabled = false;
            manual_chg_value = "";

            return "AUTO CHARGE ENABLED\nFM (? 1 2 3 4 S P X *)";
        }

        if(cmd.size() > 4 && cmd.substr(0, 4) == "CD0=")
        {
            try {
                double val = std::stod(cmd.substr(4));
                if(val < 0.05 || val > 0.50)
                    return "CD0 out of range [0.05-0.50]\nFM (? 1 2 3 4 S P X *)";
                Stanag4355::cfg_cd0 = val;
                std::stringstream ss;

                double mass_lb = 43.2 * 2.20462;
                double d_in = 0.155 * 39.3701;
                double i_form = val / 0.230;
                double bc = mass_lb / (i_form * d_in * d_in);
                ss << "CD0=" << std::fixed << std::setprecision(4) << val
                   << " (BC=" << std::setprecision(2) << bc << " lb/in2)\n";
                ss << "FM (? 1 2 3 4 S P X *)";
                return ss.str();
            } catch(...) {
                return "CD0: invalid value\nFM (? 1 2 3 4 S P X *)";
            }
        }

        if(cmd.size() > 3 && cmd.substr(0, 3) == "V0_")
        {
            size_t eq = cmd.find('=');
            if(eq == std::string::npos)
                return "V0 syntax: V0_6W=500\nFM (? 1 2 3 4 S P X *)";
            try {
                std::string chg = cmd.substr(3, eq - 3);
                double val = std::stod(cmd.substr(eq + 1));
                if(val < 100 || val > 1200)
                    return "V0 out of range [100-1200]\nFM (? 1 2 3 4 S P X *)";
                Stanag4355::cfg_v0[chg] = val;
                std::stringstream ss;
                ss << "V0_" << chg << "=" << std::fixed << std::setprecision(1) << val << " m/s\n";
                ss << "FM (? 1 2 3 4 S P X *)";
                return ss.str();
            } catch(...) {
                return "V0: invalid value\nFM (? 1 2 3 4 S P X *)";
            }
        }

        if(cmd.size() > 5 && cmd.substr(0, 5) == "TEMP=")
        {
            try {
                double val = std::stod(cmd.substr(5));
                if(val < -50.0 || val > 60.0)
                    return "TEMP out of range [-50 to 60] °C\nFM (? 1 2 3 4 S P X *)";
                Stanag4355::cfg_temp = val;
                std::stringstream ss;
                ss << "TEMP=" << std::fixed << std::setprecision(1) << val << " °C\n";
                ss << "FM (? 1 2 3 4 S P X *)";
                return ss.str();
            } catch(...) {
                return "TEMP: invalid value\nFM (? 1 2 3 4 S P X *)";
            }
        }

        if(cmd.size() > 4 && cmd.substr(0, 4) == "HUM=")
        {
            try {
                double val = std::stod(cmd.substr(4));
                if(val < 0.0 || val > 100.0)
                    return "HUM out of range [0-100] %\nFM (? 1 2 3 4 S P X *)";
                Stanag4355::cfg_humidity = val;
                std::stringstream ss;
                ss << "HUM=" << std::fixed << std::setprecision(1) << val << " %\n";
                ss << "FM (? 1 2 3 4 S P X *)";
                return ss.str();
            } catch(...) {
                return "HUM: invalid value\nFM (? 1 2 3 4 S P X *)";
            }
        }

        if(cmd.size() > 9 && cmd.substr(0, 9) == "WIND_DIR=")
        {
            try {
                double val = std::stod(cmd.substr(9));
                if(val < 0.0 || val >= 360.0)
                    return "WIND_DIR out of range [0-359] deg\nFM (? 1 2 3 4 S P X *)";
                Stanag4355::cfg_wind_dir = val;
                std::stringstream ss;
                ss << "WIND_DIR=" << std::fixed << std::setprecision(0) << val << " deg\n";
                ss << "FM (? 1 2 3 4 S P X *)";
                return ss.str();
            } catch(...) {
                return "WIND_DIR: invalid value\nFM (? 1 2 3 4 S P X *)";
            }
        }

        if(cmd.size() > 9 && cmd.substr(0, 9) == "WIND_SPD=")
        {
            try {
                double val = std::stod(cmd.substr(9));
                if(val < 0.0 || val > 50.0)
                    return "WIND_SPD out of range [0-50] m/s\nFM (? 1 2 3 4 S P X *)";
                Stanag4355::cfg_wind_spd = val;
                std::stringstream ss;
                ss << "WIND_SPD=" << std::fixed << std::setprecision(1) << val << " m/s\n";
                ss << "FM (? 1 2 3 4 S P X *)";
                return ss.str();
            } catch(...) {
                return "WIND_SPD: invalid value\nFM (? 1 2 3 4 S P X *)";
            }
        }

        if(cmd.size() > 10 && cmd.substr(0, 10) == "FIRING_AZ=")
        {
            try {
                double val = std::stod(cmd.substr(10));
                if(val < 0.0 || val >= 360.0)
                    return "FIRING_AZ out of range [0-359] deg\nFM (? 1 2 3 4 S P X *)";
                Stanag4355::cfg_firing_az = val;
                std::stringstream ss;
                ss << "FIRING_AZ=" << std::fixed << std::setprecision(0) << val << " deg\n";
                ss << "FM (? 1 2 3 4 S P X *)";
                return ss.str();
            } catch(...) {
                return "FIRING_AZ: invalid value\nFM (? 1 2 3 4 S P X *)";
            }
        }

        if(cmd == "SHOW")
        {
            std::stringstream ss;
            ss << "STANAG Config:\n";
            ss << "  CD0=" << std::fixed << std::setprecision(4) << Stanag4355::cfg_cd0 << "\n";
            ss << "  TEMP=" << std::setprecision(1) << Stanag4355::cfg_temp << " C\n";
            ss << "  HUM=" << std::setprecision(1) << Stanag4355::cfg_humidity << " %\n";
            ss << "  WIND_DIR=" << std::setprecision(0) << Stanag4355::cfg_wind_dir << " deg\n";
            ss << "  WIND_SPD=" << std::setprecision(1) << Stanag4355::cfg_wind_spd << " m/s\n";
            ss << "  FIRING_AZ=" << std::setprecision(0) << Stanag4355::cfg_firing_az << " deg\n";
            if(Stanag4355::cfg_v0.empty()) {
                ss << "  V0: (all defaults)\n";
            } else {
                for(const auto& kv : Stanag4355::cfg_v0) {
                    ss << "  V0_" << kv.first << "=" << std::setprecision(1) << kv.second << "\n";
                }
            }
            ss << "FM (? 1 2 3 4 S P X *)";
            return ss.str();
        }

        if(cmd=="STANAG")
        {
            if(ammo_proj_prop.empty() || guns.empty() || (tgt_e == 0 && tgt_n == 0))
                return "NEED COB + AMMO + TARGET first\nFM (? 1 2 3 4 S P X *)";

            int bp = base_piece_index;
            double dx = tgt_e - guns[bp].e;
            double dy = tgt_n - guns[bp].n;
            double dist_calc = std::sqrt(dx*dx + dy*dy);

            std::string chg_resolved = "";
            double qe_r, tof_r, drift_r;
            solve(ammo_proj_prop, ammo_proj_lot, dist_calc, chg_resolved, qe_r, tof_r, drift_r);

            if(chg_resolved.empty())
                return "SOLVER: NO DATA for this range\nFM (? 1 2 3 4 S P X *)";

            return stanagCompare(std::stoi(artillery_type), ammo_proj_prop, ammo_proj_lot,
                                 chg_resolved, dist_calc) + "\nFM (? 1 2 3 4 S P X *)";
        }

        if(cmd=="STANAG_CAL")
        {
            return stanagCalibrate() + "\nFM (? 1 2 3 4 S P X *)";
        }

        if(cmd=="1")
        {
            mission_type = "AREA FIRE";
            current_menu = "FM1_TGT";
            input_stage = 0;

            return "TGT (? 1 2 3 P *)";
        }

        if(cmd=="2")
        {
            fm1_base_def_reverse = false;
            active_fm1_def_reverse = false;
            active_fm1_transport_qe_shape = false;

            current_menu="REG";

            if(reg_data_available)
            {
                input_stage = -1;
                return "USE LAST REG DATA? (Y/N)";
            }

            input_stage = 0;
            return "KNPT #:";
        }

        if(cmd=="3")    
        {
            current_menu = "SHIFT_PREV_DIR";
            return "PREV DIR:";
        }

        if(cmd=="4")   
        {
            current_menu = "FM4_LR";
            return "IMPACT L/R (ej: L50 o R50):";
        }

        if(cmd=="S")
        {
            current_menu="SHEAF";
            return "SHEAF (CONV/OPEN):";
        }

        if(cmd=="R")
        {
            std::stringstream out;

            for(size_t i=0;i<guns.size();i++)
            {
                out<<"COB "<<i+1<<" ALT "<<guns[i].alt
                <<" N "<<guns[i].n
                <<" E "<<guns[i].e<<"\n";
            }
            out << "BASE PIECE " << (base_piece_index + 1) << "\n";
            out << "AZ LAY " << az_lay << "\n";
            out << "REF DEF " << def_base << "\n";

            out<<"TGT ALT "<<tgt_alt
            <<" N "<<tgt_n
            <<" E "<<tgt_e<<"\n";

            out<<"AMMO "<<ammo_proj_prop
            <<" LOT "<<ammo_proj_lot
            <<" WT "<<ammo_proj_wt<<"\n";

            if(observers.empty())
            {
                out << "OBS " << BasicEngine::obs_id
                    << " E " << obs_e
                    << " N " << obs_n
                    << " ALT " << obs_alt
                    << " GZ " << BasicEngine::obs_gz << "\n";
            }
            else
            {
                for(const auto& item : observers)
                {
                    const ObserverData& obs = item.second;

                    out << "OBS " << obs.id
                        << " E " << obs.e
                        << " N " << obs.n
                        << " ALT " << obs.alt
                        << " GZ " << obs.gz << "\n";
                }
            }

            out<<"MAP EMAX "<<map_e_max
            <<" EMIN "<<map_e_min
            <<" NMAX "<<map_n_max
            <<" NMIN "<<map_n_min<<"\n";

            return out.str() + "FM (? 1 2 3 4 R E P X *)";
        }

        if(cmd=="A")
        {
            all_guns_command = true;
            ffe_mode = true;

            return "FFE\nPRESS X";
        }

       if(cmd=="T")
        {
            current_menu="SHIFT_PREV_DIR";
            return "PREV DIR:";
        }

       if(cmd=="X")
        {

            bool has_cob = !guns.empty();
            bool has_target = !(tgt_e == 0 && tgt_n == 0);
            bool has_ammo = !ammo_proj_prop.empty();

            if(!(has_cob && has_target && has_ammo))
            {
                std::stringstream msg;

                msg << "DATA INCOMPLETE\nMISSING:";

                if(!has_cob)
                    msg << " COB";

                if(!has_target)
                    msg << " TARGET";

                if(!has_ammo)
                    msg << " AMMO";

                msg << "\nMAIN (? 1 3 4 5 7 X *)";

                current_menu = "MAIN";
                return msg.str();
            }

            std::string result = renderFire(ffe_mode);

            all_guns_command = false; 

            ShotLog shot;

            std::string label;

            switch(fire_phase)
            {
                case 0: label = "BASE FIRE"; break;
                case 1: label = "REGISTRATION"; break;
                case 2: label = "TIME REG"; break;
                case 3: label = "SHIFT (FM3)"; break;
                case 4: label = "PMI (FM4)"; break;
                default: label = "FIRE"; break;
            }
            shot.label = label;
            shot.fire = result;
            shot.timestamp = getCurrentTime();

            std::stringstream input_ss;

            for(const auto& s : last_inputs)
                input_ss << s << "\n";

            shot.inputs = input_ss.str();

            last_inputs.clear();

            mission_log.push_back(shot);
            if(fire_phase < 4)
            fire_phase++;

            last_solution = result;
            current_menu = "COMP_CORR";
            ffe_mode = false;
            chg_allowed = true;
            return result + "COMP CORR (Y N P *)\nTESON EVIL";
        }

        return drPrefix("FM",fireMenu());
    }

    if(current_menu=="FM1_TGT")
    {
        if(cmd=="1")
        {
            fm1_tgt_method = 1;
            input_stage = 0;
            current_menu = "FM1_GRID";
            return "TGT/KNPT:";
        }

        if(cmd=="2")
        {
            fm1_tgt_method = 2;
            current_menu = "FM1_TRANSPORT";
            input_stage = 0;
            return "DESDE:";
        }

        if(cmd=="3")
        {
            fm1_tgt_method = 3;
            current_menu = "FM1_POLAR";
            input_stage = 0;
            return "AZ:";
        }

        if(cmd=="P")
        {
            current_menu = "FM";
            return drPrefix("FM",fireMenu());
        }

        if(cmd=="*")
        {
            current_menu = "FM";
            return drPrefix("FM",fireMenu());
        }

        return "TGT (? 1 2 3 P *)";
    }

if(current_menu=="FM1_GRID")
{

    if(cmd=="P")
    {
        if(input_stage > 0) input_stage--;

        switch(input_stage)
        {
            case 0: return "TGT/KNPT (P *): " + std::to_string(fm1_temp_knpt);
            case 1: return "E (P *): " + std::to_string((int)fm1_temp_e);
            case 2: return "N (P *): " + std::to_string((int)fm1_temp_n);
            case 3: return "ALT (P *): " + std::to_string((int)fm1_temp_alt);
        }
    }

    std::string v = normStr(cmd);

    switch(input_stage)
    {
        case 0:
            if(!v.empty()) fm1_temp_knpt = std::stoi(v);
            input_stage++;
            return "E (P *): " + std::to_string((int)fm1_temp_e);

        case 1:
            if(!v.empty()) fm1_temp_e = std::stod(v);
            input_stage++;
            return "N (P *): " + std::to_string((int)fm1_temp_n);

        case 2:
            if(!v.empty()) fm1_temp_n = std::stod(v);
            input_stage++;
            return "ALT (P *): " + std::to_string((int)fm1_temp_alt);

        case 3:
        {
            if(!v.empty())
                fm1_temp_alt = std::stod(v);

            targets[fm1_temp_knpt] = {fm1_temp_e, fm1_temp_n, fm1_temp_alt};

            current_knpt = fm1_temp_knpt;

            tgt_e = fm1_temp_e;
            tgt_n = fm1_temp_n;
            tgt_alt = fm1_temp_alt;

            last_inputs.push_back("FM1 GRID");
            last_inputs.push_back("TGT " + std::to_string(fm1_temp_knpt));
            last_inputs.push_back("E " + std::to_string((int)fm1_temp_e));
            last_inputs.push_back("N " + std::to_string((int)fm1_temp_n));
            last_inputs.push_back("ALT " + std::to_string((int)fm1_temp_alt));

            input_stage = 0;

            bool has_cob = !guns.empty();
            bool has_target = !(tgt_e == 0 && tgt_n == 0);
            bool has_ammo = !ammo_proj_prop.empty();

            if(!(has_cob && has_target && has_ammo))
            {
                current_menu = "FM";

                std::stringstream msg;
                msg << "DATA INCOMPLETE\nMISSING:";

                if(!has_cob)
                    msg << " COB";

                if(!has_target)
                    msg << " TARGET";

                if(!has_ammo)
                    msg << " AMMO";

                msg << "\n";
                msg << drPrefix("FM", fireMenu());

                return msg.str();
            }

            fm1_grid_reg_dist_backup = reg_dist;
            fm1_grid_reg_def_backup = reg_def;
            fm1_grid_df_corr_backup = df_corr;
            fm1_grid_time_reg_correction_backup = time_reg_correction;
            fm1_grid_ud_active_backup = ud_active;
            fm1_grid_ud_corr_backup = ud_corr;

            reg_dist = 0.0;
            reg_def = 0.0;
            df_corr = 0.0;
            time_reg_correction = 0.0;
            ud_active = false;
            ud_corr = 0.0;

            fm1_grid_clean_fire_pending = true;

            active_fm1_transport_qe_shape = false;
            fm1_base_def_reverse = true;
            active_fm1_def_reverse = true;

            std::string result = finishTgtBaseShot();

            return result;
        }
    }

    return "TGT/KNPT (P *): " + std::to_string(fm1_temp_knpt);
}

if(current_menu=="FM1_TRANSPORT")
{

    if(cmd=="P")
    {
        if(input_stage > 0) input_stage--;

        switch(input_stage)
        {
            case 0: return "DESDE (P *): " + std::to_string(fm1_from_knpt);
            case 1: return "AZ (P *): " + std::to_string((int)fm1_tr_az);
            case 2: return std::string("I/D (P *): ") + fm1_tr_lr;
            case 3: return "VAL I/D (P *): " + std::to_string((int)fm1_tr_lr_val);
            case 4: return std::string("+/- (P *): ") + fm1_tr_ad;
            case 5: return "VAL +/- (P *): " + std::to_string((int)fm1_tr_ad_val);
            case 6: return std::string("S/B (P *): ") + fm1_tr_ud;
            case 7: return "VAL S/B (P *): " + std::to_string((int)fm1_tr_ud_val);
        }
    }

    std::string v = normStr(cmd);

    switch(input_stage)
    {
        case 0:
            if(!v.empty()) fm1_from_knpt = std::stoi(v);

            if(targets.find(fm1_from_knpt) == targets.end())
                return "INVALID TGT\nDESDE (P *):";

            input_stage++;
            return "AZ (P *): " + std::to_string((int)fm1_tr_az);

        case 1:
            if(!v.empty()) fm1_tr_az = std::stod(v);
            input_stage++;
            return std::string("I/D (P *): ") + fm1_tr_lr;

        case 2:
            if(!v.empty()) fm1_tr_lr = toupper(v[0]);
            input_stage++;
            return "VAL I/D (P *): " + std::to_string((int)fm1_tr_lr_val);

        case 3:
            if(!v.empty()) fm1_tr_lr_val = std::stod(v);
            input_stage++;
            return std::string("+/- (P *): ") + fm1_tr_ad;

        case 4:
            if(!v.empty()) fm1_tr_ad = v[0];
            input_stage++;
            return "VAL +/- (P *): " + std::to_string((int)fm1_tr_ad_val);

        case 5:
            if(!v.empty()) fm1_tr_ad_val = std::stod(v);
            input_stage++;
            return std::string("S/B (P *): ") + fm1_tr_ud;

        case 6:
            if(!v.empty()) fm1_tr_ud = toupper(v[0]);
            input_stage++;
            return "VAL S/B (P *): " + std::to_string((int)fm1_tr_ud_val);

        case 7:
        {
            if(!v.empty())
                fm1_tr_ud_val = std::stod(v);

            auto base = targets[fm1_from_knpt];

            double az_rad = fm1_tr_az * (PI / 3200.0);

            double dx = 0.0;
            double dy = 0.0;

            double dist = (fm1_tr_ad == '+') ? fm1_tr_ad_val : -fm1_tr_ad_val;

            dx += dist * std::sin(az_rad);
            dy += dist * std::cos(az_rad);

            double lat = (fm1_tr_lr == 'D') ? fm1_tr_lr_val : -fm1_tr_lr_val;

            dx += lat * std::cos(az_rad);
            dy -= lat * std::sin(az_rad);

            double new_e = base.e + dx;
            double new_n = base.n + dy;

            double new_alt = base.alt;

            if(fm1_tr_ud == 'S')
                new_alt += fm1_tr_ud_val;

            if(fm1_tr_ud == 'B')
                new_alt -= fm1_tr_ud_val;

            tgt_e = new_e;
            tgt_n = new_n;
            tgt_alt = new_alt;

            current_knpt = 0;

            last_inputs.push_back("FM1 TRANSPORT");
            last_inputs.push_back("DESDE " + std::to_string(fm1_from_knpt));
            last_inputs.push_back("AZ " + std::to_string((int)fm1_tr_az));

            if(fm1_tr_lr == 'I')
                last_inputs.push_back("I " + std::to_string((int)fm1_tr_lr_val));
            else if(fm1_tr_lr == 'D')
                last_inputs.push_back("D " + std::to_string((int)fm1_tr_lr_val));

            if(fm1_tr_ad == '+')
                last_inputs.push_back("+ " + std::to_string((int)fm1_tr_ad_val));
            else
                last_inputs.push_back("- " + std::to_string((int)fm1_tr_ad_val));

            if(fm1_tr_ud == 'S')
                last_inputs.push_back("S " + std::to_string((int)fm1_tr_ud_val));
            else if(fm1_tr_ud == 'B')
                last_inputs.push_back("B " + std::to_string((int)fm1_tr_ud_val));

            input_stage = 0;

            bool has_cob = !guns.empty();
            bool has_target = !(tgt_e == 0 && tgt_n == 0);
            bool has_ammo = !ammo_proj_prop.empty();

            if(!(has_cob && has_target && has_ammo))
            {
                current_menu = "FM";

                std::stringstream msg;
                msg << "DATA INCOMPLETE\nMISSING:";

                if(!has_cob)
                    msg << " COB";

                if(!has_target)
                    msg << " TARGET";

                if(!has_ammo)
                    msg << " AMMO";

                msg << "\n";
                msg << drPrefix("FM", fireMenu());

                return msg.str();
            }

            fm1_base_def_reverse = true;
            active_fm1_def_reverse = true;
            active_fm1_transport_qe_shape = true;

            std::string result = finishTgtBaseShot();

            return result;
        }
    }

    return "DESDE (P *): " + std::to_string(fm1_from_knpt);
}

if(current_menu=="FM1_POLAR")
{

    if(cmd=="P")
    {
        if(input_stage > 0) input_stage--;

        if(input_stage==0)
            return "AZ (P *): " + std::to_string((int)fm1_pol_az);

        if(input_stage==1)
            return "DIST (P *): " + std::to_string((int)fm1_pol_dist);

        if(input_stage==2)
            return std::string("S/B (P *): ") + fm1_pol_ud;

        if(input_stage==3)
            return "VAL S/B (P *): " + std::to_string((int)fm1_pol_ud_val);
    }

    std::string v = normStr(cmd);

    switch(input_stage)
    {
        case 0:
            if(!v.empty()) fm1_pol_az = std::stod(v);
            input_stage++;
            return "DIST (P *): " + std::to_string((int)fm1_pol_dist);

        case 1:
            if(!v.empty()) fm1_pol_dist = std::stod(v);
            input_stage++;
            return std::string("S/B (P *): ") + fm1_pol_ud;

        case 2:
            if(!v.empty()) fm1_pol_ud = toupper(v[0]);
            input_stage++;
            return "VAL S/B (P *): " + std::to_string((int)fm1_pol_ud_val);

        case 3:
        {
            if(!v.empty())
                fm1_pol_ud_val = std::stod(v);

            double az_rad = fm1_pol_az * (PI / 3200.0);

            double dx = fm1_pol_dist * std::sin(az_rad);
            double dy = fm1_pol_dist * std::cos(az_rad);

            double new_e = gb_e + dx;
            double new_n = gb_n + dy;

            double new_alt = gb_alt;

            if(fm1_pol_ud == 'S')
                new_alt += fm1_pol_ud_val;

            if(fm1_pol_ud == 'B')
                new_alt -= fm1_pol_ud_val;

            tgt_e = new_e;
            tgt_n = new_n;
            tgt_alt = new_alt;

            if(current_knpt <= 0)
                current_knpt = 1;

            targets[current_knpt] = {tgt_e, tgt_n, tgt_alt};

            last_inputs.push_back("FM1 POLAR");
            last_inputs.push_back("AZ " + std::to_string((int)fm1_pol_az));
            last_inputs.push_back("DIST " + std::to_string((int)fm1_pol_dist));

            if(fm1_pol_ud == 'S')
                last_inputs.push_back("S " + std::to_string((int)fm1_pol_ud_val));
            else if(fm1_pol_ud == 'B')
                last_inputs.push_back("B " + std::to_string((int)fm1_pol_ud_val));

            input_stage = 0;

            bool has_cob = !guns.empty();
            bool has_target = !(tgt_e == 0 && tgt_n == 0);
            bool has_ammo = !ammo_proj_prop.empty();

            if(!(has_cob && has_target && has_ammo))
            {
                current_menu = "FM";

                std::stringstream msg;
                msg << "DATA INCOMPLETE\nMISSING:";

                if(!has_cob)
                    msg << " COB";

                if(!has_target)
                    msg << " TARGET";

                if(!has_ammo)
                    msg << " AMMO";

                msg << "\n";
                msg << drPrefix("FM", fireMenu());

                return msg.str();
            }

            return finishTgtBaseShot();
        }
    }

    return "AZ (P *): " + std::to_string((int)fm1_pol_az);
}

    static std::vector<double> fm4_lr;
    static std::vector<double> fm4_ad;
    static std::vector<double> fm4_ud;

    if(current_menu=="FM4_LR")
    {
        if(cmd=="X")
        {
            current_menu="FM";
            return "FM (? 1 2 3 4 S P X *)";
        }

        std::string dir = cmd.substr(0,1);
        double val = std::stod(cmd.substr(1));

        if(dir=="L")
        {
            fm4_lr.push_back(-val);
            last_inputs.push_back("L" + std::to_string((int)val));
        }
        else if(dir=="R")
        {
            fm4_lr.push_back(val);
            last_inputs.push_back("R" + std::to_string((int)val));
        }

        current_menu="FM4_AD";
        return "IMPACT A/D (A o D):";
    }

    if(current_menu=="FM4_AD")
    {
        std::string dir = cmd.substr(0,1);
        double val = std::stod(cmd.substr(1));

        if(dir=="A")
        {
            fm4_ad.push_back(val);
            last_inputs.push_back("A" + std::to_string((int)val));
        }
        else if(dir=="D")
        {
            fm4_ad.push_back(-val);
            last_inputs.push_back("D" + std::to_string((int)val));
        }

        current_menu="FM4_UD";
        return "IMPACT U/D (U o D):";
    }

    if(current_menu=="FM4_UD")
    {
        std::string dir = cmd.substr(0,1);
        double val = std::stod(cmd.substr(1));

        if(dir=="U")
        {
            fm4_ud.push_back(val);
            last_inputs.push_back("U" + std::to_string((int)val));
        }
        else if(dir=="D")
        {
            fm4_ud.push_back(-val);
            last_inputs.push_back("D" + std::to_string((int)val));
        }

        current_menu="FM4_NEXT";
        return "ADD MORE? (Y/N)";
    }

    if(current_menu=="FM4_NEXT")
    {
        if(cmd=="Y")
        {
            current_menu="FM4_LR";
            return "IMPACT L/R:";
        }

        if(cmd=="N")
        {
            fire_phase = 4;

            double avg_lr=0, avg_ad=0, avg_ud=0;

            for(double v:fm4_lr) avg_lr+=v;
            for(double v:fm4_ad) avg_ad+=v;
            for(double v:fm4_ud) avg_ud+=v;

            if(!fm4_lr.empty()) avg_lr/=fm4_lr.size();
            if(!fm4_ad.empty()) avg_ad/=fm4_ad.size();
            if(!fm4_ud.empty()) avg_ud/=fm4_ud.size();

            applyObserverCorrection(
                avg_lr,
                avg_ad,
                avg_ud,
                inst_last_dir
            );

            fm4_lr.clear();
            fm4_ad.clear();
            fm4_ud.clear();

            std::string result = renderFire(false);

            last_solution = result;

            current_menu="FM";

            return result + "FM (? 1 2 3 4 R E P X *)";
        }

        return "ADD MORE? (Y/N)";
    }

if(current_menu=="SHIFT_PREV_DIR")
{
    shift_prev_dir = cmd;
    current_menu="SHIFT_PREV_LR";
    return "PREV L/R:";
}

if(current_menu=="SHIFT_PREV_LR")
{
    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'L' || dir == 'R'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "PREV L/R:";
        }

        if(dir == 'L')
            shift_prev_lr = -val;
        else if(dir == 'R')
            shift_prev_lr = val;
        else
            shift_prev_lr = val;
    }
    else
    {
        shift_prev_lr = 0.0;
    }

    current_menu="SHIFT_PREV_AD";
    return "PREV A/D:";
}

if(current_menu=="SHIFT_PREV_AD")
{
    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'A' || dir == 'D'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "PREV A/D:";
        }

        if(dir == 'A')
            shift_prev_ad = val;
        else if(dir == 'D')
            shift_prev_ad = -val;
        else
            shift_prev_ad = val;
    }
    else
    {
        shift_prev_ad = 0.0;
    }

    current_menu="SHIFT_PREV_UD";
    return "PREV U/D:";
}

if(current_menu=="SHIFT_PREV_UD")
{
    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'U' || dir == 'D'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "PREV U/D:";
        }

        if(dir == 'U')
            shift_prev_ud = val;
        else if(dir == 'D')
            shift_prev_ud = -val;
        else
            shift_prev_ud = val;
    }
    else
    {
        shift_prev_ud = 0.0;
    }

    current_menu="SHIFT_DIR";
    return "DIR:";
}

if(current_menu=="SHIFT_DIR")
{
    std::string v = normStr(cmd);

    if(!v.empty())
        shift_new_dir = v;

    double dir_val = 0.0;

    try
    {
        dir_val = std::stod(shift_new_dir);
    }
    catch(...)
    {
        dir_val = inst_last_dir;
    }

    while(dir_val < 0.0)
        dir_val += 6400.0;

    while(dir_val >= 6400.0)
        dir_val -= 6400.0;

    shift_new_dir = std::to_string((int)std::round(dir_val));
    inst_last_dir = dir_val;

    shift_angle = computeAngleTFromDir(dir_val);

    current_menu="SHIFT_ANGLE";

    std::stringstream ss;
    ss << "ANGLE T: " << (int)std::round(shift_angle);
    return ss.str();
}

if(current_menu=="SHIFT_ANGLE")
{
    std::string v = normStr(cmd);

    if(!v.empty())
        shift_angle = std::stod(v);

    current_menu="SHIFT_LR";
    return "L/R SHIFT:";
}

if(current_menu=="SHIFT_LR")
{
    std::string v = normStr(cmd);
    double val = 0.0;

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);

        try
        {
            if(v.size() > 1 && (dir == 'L' || dir == 'R'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "L/R SHIFT:";
        }

        if(dir == 'L')
            shift_prev_lr = -val;
        else if(dir == 'R')
            shift_prev_lr = val;
        else
            shift_prev_lr = val;
    }
    else
    {
        shift_prev_lr = 0.0;
    }

    current_menu="SHIFT_AD";
    return "A/D SHIFT:";
}

if(current_menu=="SHIFT_AD")
{
    std::string v = normStr(cmd);
    double val = 0.0;

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);

        try
        {
            if(v.size() > 1 && (dir == 'A' || dir == 'D'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "A/D SHIFT:";
        }

        if(dir == 'A')
            shift_prev_ad = val;
        else if(dir == 'D')
            shift_prev_ad = -val;
        else
            shift_prev_ad = val;
    }
    else
    {
        shift_prev_ad = 0.0;
    }

    current_menu="SHIFT_UD";
    return "U/D SHIFT:";
}

if(current_menu=="SHIFT_UD")
{
    std::string v = normStr(cmd);
    double val = 0.0;

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);

        try
        {
            if(v.size() > 1 && (dir == 'U' || dir == 'D'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "U/D SHIFT:";
        }

        if(dir == 'U')
            shift_prev_ud = val;
        else if(dir == 'D')
            shift_prev_ud = -val;
        else
            shift_prev_ud = val;
    }
    else
    {
        shift_prev_ud = 0.0;
    }

    double dir_val = 0.0;

    try
    {
        dir_val = std::stod(shift_new_dir);
    }
    catch(...)
    {
        dir_val = inst_last_dir;
    }

    applyObserverCorrection(
        shift_prev_lr,
        shift_prev_ad,
        shift_prev_ud,
        dir_val
    );

    fire_phase = 3;

    current_menu="FM";

    return "SHIFT APPLIED\nFM (? 1 2 3 4 S P X *)";
}

if(current_menu=="SHIFT")
{
    std::string v = normStr(cmd);

    if(v.empty())
        return "SHIFT (L/R A/D U/D):";

    char dir = (char)std::toupper((unsigned char)v[0]);
    double val = 0.0;

    try
    {
        if(v.size() > 1 && (dir == 'L' || dir == 'R' || dir == 'A' || dir == 'D' || dir == 'U'))
            val = std::stod(v.substr(1));
        else
            val = std::stod(v);
    }
    catch(...)
    {
        return "SHIFT (L/R A/D U/D):";
    }

    double obs_lr = 0.0;
    double obs_ad = 0.0;
    double obs_ud = 0.0;

    if(dir == 'L')
        obs_lr = -val;
    else if(dir == 'R')
        obs_lr = val;
    else if(dir == 'A')
        obs_ad = val;
    else if(dir == 'D')
        obs_ad = -val;
    else if(dir == 'U')
        obs_ud = val;
    else
        return "SHIFT (L/R A/D U/D):";

    applyObserverCorrection(
        obs_lr,
        obs_ad,
        obs_ud,
        inst_last_dir
    );

    current_menu="FM";
    return "SHIFT APPLIED\nFM (? 1 2 3 4 S P X *)";
}

    if(current_menu=="SHEAF")
    {
        if(cmd=="CONV")
        {
            sheaf_mode="CONV";
            current_menu="FM";
            return "SHEAF CONVERGED\nFM (? 1 2 3 4 S P X *)";
        }

        if(cmd=="OPEN")
        {
            sheaf_mode="OPEN";
            current_menu="SHEAF_WIDTH"; 
            return "OPEN WIDTH (MILS):";
        }

        return "SHEAF (CONV/OPEN):";
    }

    if(current_menu=="SHEAF_WIDTH")
    {
        try
        {
            sheaf_width = std::stod(cmd);

            current_menu="FM";

            std::stringstream ss;
            ss << "SHEAF OPEN WIDTH " << sheaf_width << "\n";
            ss << "FM (? 1 2 3 4 S P X *)";

            return ss.str();
        }
        catch(...)
        {
            return "OPEN WIDTH (MILS):";
        }
    }

   if(current_menu=="REG")
{

    if(input_stage == -1)
    {
        if(cmd == "Y")
        {
            current_knpt = last_knpt;

            tgt_e = targets[last_knpt].e;
            tgt_n = targets[last_knpt].n;
            tgt_alt = targets[last_knpt].alt;

            ammo_proj_prop = last_proj;
            ammo_proj_lot = last_lot;
            fuze_time_mode = last_fuze_tia;

            reg_dist = last_reg_rg;
            reg_def = last_reg_def;

            fire_phase = 1;

            std::string result = renderFire(false);

            last_solution = result;
            current_menu = "COMP_CORR";
            input_stage = 0;
            chg_allowed = true;

            return result + "COMP CORR (Y N P *)\nTESON EVIL";
        }

        if(cmd == "N")
        {
            input_stage = 0;
            return "KNPT #:";
        }

        return "USE LAST REG DATA? (Y/N)";
    }
    switch(input_stage)
    {
        case 0:
        {
            int knpt = std::stoi(cmd);
            last_inputs.push_back("KNPT " + std::to_string(knpt));

            if(targets.find(knpt) == targets.end())
                return "KNPT NOT FOUND";

            current_knpt = knpt;

            tgt_e = targets[knpt].e;
            tgt_n = targets[knpt].n;
            tgt_alt = targets[knpt].alt;

            std::stringstream out;
            out << "TGT DATA:\n";
            out << "EAST: " << tgt_e << "\n";
            out << "NORTH: " << tgt_n << "\n";
            out << "ALT: " << tgt_alt << "\n";

            input_stage++;
            return out.str() + "MET ENG:";
        }

        case 1:
            input_stage++;
            return "MET CNTL:";

        case 2:
            input_stage++;
            return "PROJ:";

        case 3:
            ammo_proj_prop = cmd;
            last_inputs.push_back("PROJ " + cmd);
            input_stage++;
            return "PROJ LOT:";

        case 4:
            ammo_proj_lot = cmd;
            last_inputs.push_back("LOT " + cmd);
            input_stage++;
            return "FUZE:";

        case 5:
        {
            if(cmd=="TIA")
            {
                fuze_time_mode = true;
                hob = 0;
                last_inputs.push_back("FUZE TIA");
            }
            else
            {
                fuze_time_mode = false;
                hob = 0;
                last_inputs.push_back("FUZE PDA");
            }

            current_menu = "REG_BASE_PIECE";
            return "BASE PIECE (P *): " + std::to_string(base_piece_index + 1);
        }

        case 6:
        {
            std::string v = normStr(cmd);
            double reg_input = v.empty() ? last_dist_solution : std::stod(v);

            reg_dist = reg_input - last_dist_solution;

            last_inputs.push_back("REG DIST " + std::to_string((int)reg_input));

            try
            {
                renderFire(false);
            }
            catch(...)
            {
                return "ERROR: NO SOLUTION";
            }

            input_stage++;

            std::stringstream ss;
            ss << "REG DEF (" << (int)last_def_solution << "):";
            return ss.str();
        }

        case 7:
        {
            std::string v = normStr(cmd);
            double reg_input = v.empty() ? last_def_solution : std::stod(v);

            double reg_def_backup = reg_def;

            reg_def = 0;

            double adj_tgt_e = tgt_e;
            double adj_tgt_n = tgt_n;

            if(shift_lr != 0 || shift_ad != 0)
            {
                double dir_rad = shift_angle * (2.0 * PI / 6400.0);

                double ad_e = std::sin(dir_rad);
                double ad_n = std::cos(dir_rad);

                double lr_e = std::cos(dir_rad);
                double lr_n = -std::sin(dir_rad);

                double delta_e = (ad_e * shift_ad) + (lr_e * shift_lr);
                double delta_n = (ad_n * shift_ad) + (lr_n * shift_lr);

                adj_tgt_e += delta_e;
                adj_tgt_n += delta_n;
            }
            double dx = adj_tgt_e - guns[base_piece_index].e;
            double dy = adj_tgt_n - guns[base_piece_index].n;

            double az = std::atan2(dx, dy);
            if(az < 0) az += 2 * PI;

            double mils = az * (6400 / (2 * PI));
            mils = std::floor((mils + 1.5) / 2.0) * 2.0;

            double dist_geom = std::sqrt(dx*dx + dy*dy);
            double dist = dist_geom + reg_dist;

            double qe=0,tof=0,drift=0;
            std::string chg="";

            solve(ammo_proj_prop, ammo_proj_lot, dist, chg, qe, tof, drift);

            if(std::abs(drift) < 0.001)
            {
                drift = 0.00038 * dist;
            }

            double def_real = def_base - (mils - az_lay);
            def_real += df_corr;

            double drift_accum_real = drift;  
            double jump_h_real = 6.6;

            def_real += drift_accum_real;
            def_real += jump_h_real;

            def_real = std::round(def_real);

            while(def_real < 0) def_real += 6400;
            while(def_real >= 6400) def_real -= 6400;

            reg_def = reg_def_backup;

            if(std::abs(reg_input - last_def_solution) <= 1)
            {

                double max_def = -1e9;
                double min_def = 1e9;

            for(size_t i = 0; i < guns.size(); i++)
            {
                double dx = tgt_e - guns[i].e;
                double dy = tgt_n - guns[i].n;

                double dist_geom = std::sqrt(dx*dx + dy*dy);

                double az = std::atan2(dx, dy);
                if(az < 0) az += 2 * PI;

                double mils_raw = az * (6400 / (2 * PI));
                double mils = std::floor((mils_raw + 1.5) / 2.0) * 2.0;

                double dist = dist_geom + reg_dist;

                double qe_tmp=0,tof_tmp=0,drift_tmp=0;
                std::string chg_tmp="";

                solve(ammo_proj_prop, ammo_proj_lot, dist, chg_tmp, qe_tmp, tof_tmp, drift_tmp);

                if(std::abs(drift_tmp) < 0.001)
                {
                    drift_tmp = 0.00038 * dist;
                }

                double def_temp = def_base - (mils - az_lay);
                def_temp += reg_def + df_corr;

                double drift_accum = drift_tmp;  
                double jump_h = 6.6;

                def_temp += drift_accum;
                def_temp += jump_h;

                def_temp = std::round(def_temp);

                while(def_temp < 0) def_temp += 6400;
                while(def_temp >= 6400) def_temp -= 6400;

                if(def_temp > max_def) max_def = def_temp;
                if(def_temp < min_def) min_def = def_temp;
            }

                reg_def = 0.0;
            }
            else
            {
                reg_def = reg_input - last_def_solution;
            }

            last_inputs.push_back("REG DEF " + std::to_string((int)reg_input));

            reg_data_available = true;
            last_knpt = current_knpt;
            last_proj = ammo_proj_prop;
            last_lot = ammo_proj_lot;
            last_fuze_tia = fuze_time_mode;
            last_reg_rg = reg_dist;
            last_reg_def = reg_def;

            fire_phase = 1;

            std::string result = renderFire(false);

            last_solution = result;
            current_menu = "COMP_CORR";
            input_stage = 0;
            chg_allowed = true;

            return result + "COMP CORR (Y N P *)\nTESON EVIL";
        }
    }

    return "REG ERROR";
}

if(current_menu=="REG_BASE_PIECE")
{
    if(cmd=="P")
    {
        current_menu = "REG";
        input_stage = 5;
        return "FUZE:";
    }

    std::string v = normStr(cmd);

    int bp = base_piece_index + 1;

    if(!v.empty())
    {
        try
        {
            bp = std::stoi(v);
        }
        catch(...)
        {
            return "BASE PIECE (P *): " + std::to_string(base_piece_index + 1);
        }
    }

    if(bp <= 0)
    {
        return "INVALID BASE\nBASE PIECE (P *): " + std::to_string(base_piece_index + 1);
    }

    if(bp > (int)guns.size())
    {
        return "BASE > QTY\nBASE PIECE (P *): " + std::to_string(base_piece_index + 1);
    }

    base_piece_index = bp - 1;

    try
    {
        renderFire(false);
    }
    catch(...)
    {
        current_menu = "REG";
        input_stage = 5;
        return "ERROR: NO SOLUTION";
    }

    current_menu = "REG";
    input_stage = 6;

    std::stringstream ss;
    ss << "REG RG (" << (int)last_dist_solution << "):";
    return ss.str();
}

if(current_menu=="FM1_BASE_PIECE")
{
    if(cmd=="P")
    {
        current_menu = "FM";
        return drPrefix("FM", fireMenu());
    }

    std::string v = normStr(cmd);

    int bp = base_piece_index + 1;

    if(!v.empty())
    {
        try
        {
            bp = std::stoi(v);
        }
        catch(...)
        {
            return "BASE PIECE (P *): " + std::to_string(base_piece_index + 1);
        }
    }

    if(bp <= 0)
    {
        return "INVALID BASE\nBASE PIECE (P *): " + std::to_string(base_piece_index + 1);
    }

    if(bp > (int)guns.size())
    {
        return "BASE > QTY\nBASE PIECE (P *): " + std::to_string(base_piece_index + 1);
    }

    base_piece_index = bp - 1;

    std::string result = renderFire(false);

    if(fm1_grid_clean_fire_pending)
    {
        reg_dist = fm1_grid_reg_dist_backup;
        reg_def = fm1_grid_reg_def_backup;
        df_corr = fm1_grid_df_corr_backup;
        time_reg_correction = fm1_grid_time_reg_correction_backup;
        ud_active = fm1_grid_ud_active_backup;
        ud_corr = fm1_grid_ud_corr_backup;

        fm1_grid_clean_fire_pending = false;
    }

    ShotLog shot;
    shot.label = "BASE FIRE";
    shot.fire = result;
    shot.timestamp = getCurrentTime();

    std::stringstream input_ss;

    for(const auto& s : last_inputs)
        input_ss << s << "\n";

    shot.inputs = input_ss.str();

    last_inputs.clear();

    mission_log.push_back(shot);

    last_solution = result;
    current_menu = "COMP_CORR";
    chg_allowed = true;

    fm1_base_def_reverse = false;
    active_fm1_def_reverse = false;
    active_fm1_transport_qe_shape = false;

    return result + "COMP CORR (Y N P *)\nTESON EVIL";
}

if(current_menu=="COMP_CORR")
{

    if((cmd=="" || cmd=="X") && chg_allowed)
    {
        chg_allowed = false;
        chg_edit_mode = true;
        chg_wait_value = false;
        current_menu = "CHG_EDIT";
        return "LOT: ?";
    }

    if(cmd=="Y")
    {
        current_menu = "TIME_REG";
        return "TIME REG (Y N P *)";
    }

if(cmd=="N")
{
    if(time_reg_fuze_temporary)
    {
        fuze_time_mode = false;
        hob = 0;
        time_reg_fuze_temporary = false;
    }

    ud_active = false;
    ud_corr = 0.0;

    inst_prev_dir = 0.0;
    inst_prev_lr = 0.0;
    inst_prev_ad = 0.0;
    inst_prev_ud = 0.0;

    inst_new_dir = inst_last_dir;
    inst_angle_t = 0.0;

    inst_lr_shift = 0.0;
    inst_ad_shift = 0.0;
    inst_ud_shift = 0.0;

    current_menu = "INST_PREV_DIR";
    return "PREV DIR (*):";
}

    if(cmd=="P")
    {
        current_menu = "FM";
        return drPrefix("FM", fireMenu());
    }

    return "COMP CORR (Y N P *)";
}

if(current_menu=="INST_PREV_DIR")
{
    if(cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    std::string v = normStr(cmd);

    if(!v.empty())
        inst_prev_dir = std::stod(v);

    current_menu = "INST_PREV_LR";
    return "PREV L/R (P*):";
}

if(current_menu=="INST_PREV_LR")
{
    if(cmd=="P")
    {
        current_menu = "INST_PREV_DIR";
        return "PREV DIR (*):";
    }

    if(cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'L' || dir == 'R'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "PREV L/R (P*):";
        }

        if(dir == 'L')
            inst_prev_lr = -val;
        else if(dir == 'R')
            inst_prev_lr = val;
        else
            inst_prev_lr = val;
    }

    current_menu = "INST_PREV_AD";
    return "PREV A/D (P*):";
}

if(current_menu=="INST_PREV_AD")
{
    if(cmd=="P")
    {
        current_menu = "INST_PREV_LR";
        return "PREV L/R (P*):";
    }

    if(cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'A' || dir == 'D'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "PREV A/D (P*):";
        }

        if(dir == 'A')
            inst_prev_ad = val;
        else if(dir == 'D')
            inst_prev_ad = -val;
        else
            inst_prev_ad = val;
    }

    current_menu = "INST_PREV_UD";
    return "PREV U/D (P*):";
}

if(current_menu=="INST_PREV_UD")
{
    if(cmd=="P")
    {
        current_menu = "INST_PREV_AD";
        return "PREV A/D (P*):";
    }

    if(cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'U' || dir == 'D'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "PREV U/D (P*):";
        }

        if(dir == 'U')
            inst_prev_ud = val;
        else if(dir == 'D')
            inst_prev_ud = -val;
        else
            inst_prev_ud = val;
    }

    current_menu = "INST_DIR";

    inst_new_dir = inst_last_dir;

    std::stringstream ss;
    ss << "DIR (P X): " << (int)std::round(inst_new_dir);

    return ss.str();
}

if(current_menu=="INST_DIR")
{
    if(cmd=="P")
    {
        current_menu = "INST_PREV_UD";
        return "PREV U/D (P*):";
    }

    if(cmd=="X" || cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    std::string v = normStr(cmd);

    if(!v.empty())
    {
        inst_new_dir = std::stod(v);
        inst_last_dir = inst_new_dir;
    }
    else
    {
        inst_new_dir = inst_last_dir;
    }

    while(inst_new_dir < 0)
        inst_new_dir += 6400.0;

    while(inst_new_dir >= 6400.0)
        inst_new_dir -= 6400.0;

    inst_last_dir = inst_new_dir;

    inst_angle_t = computeAngleTFromDir(inst_new_dir);

    current_menu = "INST_ANGLE_T";

    std::stringstream ss;
    ss << "ANG T (P*): " << (int)std::round(inst_angle_t);

    return ss.str();
}

if(current_menu=="INST_ANGLE_T")
{
    if(cmd=="P")
    {
        current_menu = "INST_DIR";

        std::stringstream ss;
        ss << "DIR (P X): " << (int)std::round(inst_last_dir);

        return ss.str();
    }

    if(cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    inst_angle_t = computeAngleTFromDir(inst_new_dir);

    current_menu = "INST_LR_SHIFT";
    return "(L/R) SHIFT (P*):";
}

if(current_menu=="INST_LR_SHIFT")
{
    if(cmd=="P")
    {
        current_menu = "INST_ANGLE_T";

        inst_angle_t = computeAngleTFromDir(inst_new_dir);

        std::stringstream ss;
        ss << "ANG T (P*): " << (int)std::round(inst_angle_t);

        return ss.str();
    }

    if(cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'L' || dir == 'R'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "(L/R) SHIFT (P*):";
        }

        if(dir == 'L')
            inst_lr_shift = -val;
        else if(dir == 'R')
            inst_lr_shift = val;
        else
            inst_lr_shift = val;
    }

    current_menu = "INST_AD_SHIFT";
    return "(A/D) SHIFT (P*):";
}

if(current_menu=="INST_AD_SHIFT")
{
    if(cmd=="P")
    {
        current_menu = "INST_LR_SHIFT";
        return "(L/R) SHIFT (P*):";
    }

    if(cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'A' || dir == 'D'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "(A/D) SHIFT (P*):";
        }

        if(dir == 'A')
            inst_ad_shift = val;
        else if(dir == 'D')
            inst_ad_shift = -val;
        else
            inst_ad_shift = val;
    }

    current_menu = "INST_UD_SHIFT";
    return "(U/D) SHIFT (P*):";
}

if(current_menu=="INST_UD_SHIFT")
{
    if(cmd=="P")
    {
        current_menu = "INST_AD_SHIFT";
        return "(A/D) SHIFT (P*):";
    }

    if(cmd=="*")
    {
        current_menu = "COMP_CORR";
        return "COMP CORR (Y N P *)";
    }

    std::string v = normStr(cmd);

    if(!v.empty())
    {
        char dir = (char)std::toupper((unsigned char)v[0]);
        double val = 0.0;

        try
        {
            if(v.size() > 1 && (dir == 'U' || dir == 'D'))
                val = std::stod(v.substr(1));
            else
                val = std::stod(v);
        }
        catch(...)
        {
            return "(U/D) SHIFT (P*):";
        }

        if(dir == 'U')
            inst_ud_shift = val;
        else if(dir == 'D')
            inst_ud_shift = -val;
        else
            inst_ud_shift = val;
    }

    applyObserverCorrection(
        inst_lr_shift,
        inst_ad_shift,
        inst_ud_shift,
        inst_new_dir
    );
    fire_phase = 3;

    std::string result = renderFire(false);

    ShotLog shot;
    shot.label = "SHIFT (FM3)";
    shot.fire = result;
    shot.timestamp = getCurrentTime();

    std::stringstream input_ss;
    input_ss << "ESP INSTANTANEA\n";
    input_ss << "PREV DIR " << (int)std::round(inst_prev_dir) << "\n";
    input_ss << "PREV LR " << (int)std::round(inst_prev_lr) << "\n";
    input_ss << "PREV AD " << (int)std::round(inst_prev_ad) << "\n";
    input_ss << "PREV UD " << (int)std::round(inst_prev_ud) << "\n";
    input_ss << "DIR " << (int)std::round(inst_new_dir) << "\n";
    input_ss << "ANG T " << (int)std::round(inst_angle_t) << "\n";
    input_ss << "LR SHIFT " << (int)std::round(inst_lr_shift) << "\n";
    input_ss << "AD SHIFT " << (int)std::round(inst_ad_shift) << "\n";
    input_ss << "UD SHIFT " << (int)std::round(inst_ud_shift) << "\n";

    shot.inputs = input_ss.str();

    mission_log.push_back(shot);

    last_solution = result;
    current_menu = "COMP_CORR";

    return result + "COMP CORR (Y N P *)\nTESON EVIL";
}

    if(current_menu=="TIME_REG")
    {

        if(cmd=="Y")
        {
            current_menu="TIME_REG_FUZE";
            fire_phase = 2;
            return std::string("FUZE (P*) ") + (fuze_time_mode ? "TIA" : "PDA");
        }

        if(cmd=="P")
        {
            current_menu="TIME_REG_INPUT";
            return "TIME CORR:";
        }

        if(cmd=="N")
        {
            current_menu="FM";
            return drPrefix("FM",fireMenu());
        }

        return "TIME REG (Y N P *)";
    }

    if(current_menu=="TIME_REG_INPUT")
    {
        time_reg_correction=std::stod(cmd);
        current_menu="FM";
        return drPrefix("FM",fireMenu());
    }

    if(current_menu=="TIME_REG_FUZE")
    {
        if(cmd=="PDA")
        {

            fuze_time_mode = false;
            hob = 0;
            time_reg_fuze_temporary = false;

            current_menu = "UD_CORR";
            return "(U/D) CORR (*)";
        }

        if(cmd=="TIA")
        {

            fuze_time_mode = true;
            hob = 0;
            time_reg_fuze_temporary = true;

            current_menu = "TIME_REG_HOB";
            return "TOTAL HOB (*):";
        }

        return std::string("FUZE (P*) ") + (fuze_time_mode ? "TIA" : "PDA");
    }

    if(current_menu=="TIME_REG_HOB")
    {
        hob = std::stod(cmd);
        last_inputs.push_back("HOB " + cmd);

        fuze_time_mode = true;
        time_reg_fuze_temporary = true;

        current_menu = "UD_CORR";
        return "(U/D) CORR (*)";
    }

    if(current_menu=="UD_CORR")
    {
        if(cmd=="X")
        {
            if(time_reg_fuze_temporary)
            {
                fuze_time_mode = false;
                hob = 0;
                time_reg_fuze_temporary = false;
            }

            ud_active = false;
            ud_corr = 0.0;

            current_menu = "FM";
            return drPrefix("FM", fireMenu());
        }

        double corr = 0.0;

        if(!parseUD(cmd, corr))
            return "(U/D) CORR (*)";

        ud_corr = corr;
        ud_active = true;

        last_inputs.push_back(cmd);

        std::string result = renderFire(false);
        last_solution = result;

        ud_active = false;
        ud_corr = 0.0;

        if(time_reg_fuze_temporary)
        {
            fuze_time_mode = false;
            hob = 0;
            time_reg_fuze_temporary = false;
        }

        current_menu = "COMP_CORR";
        chg_allowed = true;

        return result + "COMP CORR (Y N P *)\nTESON EVIL";
    }

if(current_menu=="DF_CORR")
{
    std::stringstream ss(cmd);
    std::string dir; 
    double val;

    ss >> dir >> val;

    double base_dist = last_dist_solution;

    if(base_dist <= 0)
        return "NO BASE DIST";

    double mils = (val / base_dist) * 1000.0;

    if(dir=="ADD" || dir=="DROP")
    {
        double new_dist;

    if(dir=="ADD")
    {
        reg_dist += val;
    }
    else 
    {
        reg_dist -= val;
    }

    time_reg_correction = 0;
    }

    else if(dir=="RIGHT")
    {
        df_corr += mils;
    }
    else if(dir=="LEFT")
    {
        df_corr -= mils;
    }
    else
    {
        return "DF CORR (ADD/DROP LEFT/RIGHT *)";
    }

    current_menu="FM";
    return drPrefix("FM",fireMenu());
}

if(current_menu=="CHG_EDIT")
{

    if(!chg_wait_value)
    {
        chg_wait_value = true;
        return "Cg:";
    }

    try
    {
        manual_chg_enabled = true;
        manual_chg_value = cmd;

        last_inputs.push_back("CHG " + cmd);

        chg_edit_mode = false;
        chg_wait_value = false;

        current_menu = "FM";

        return "CHG UPDATED\nFM (? 1 2 3 4 S P X *)";
    }
    catch(...)
    {
        return "Cg:";
    }
}

    return drPrefix("MAIN",mainMenu());

    } 

std::string BasicEngine::mainMenu(){ return "MAIN (? 1 3 4 5 7 X *)"; }
std::string BasicEngine::fireMenu(){ return "FM (? 1 2 3 4 R E P X *)"; }
std::string BasicEngine::afuMenu(){ return "AFU INDEX (? 1 3 5 *)"; }
std::string BasicEngine::ammoMenu(){ return "AMMO FILE (? I *)"; }

std::string BasicEngine::resetData()
{
    fire_phase = 0;
    current_menu="MAIN";
    input_stage=0;

    cob_alt=0;
    cob_n=0;
    cob_e=0;

    tgt_alt=0;
    tgt_n=0;
    tgt_e=0;
    current_tgt_indicator="";

    BasicEngine::obs_id = 0;
    BasicEngine::obs_gz = 0;
    obs_alt = 0;
    obs_n = 0;
    obs_e = 0;
    obs_expected_qty = 1;
    obs_current_index = 1;
    observers.clear();

    wind_dir=0;
    wind_speed=2.5;   
    temperature=28.0; 

    Stanag4355::cfg_temp = 28.0;
    Stanag4355::cfg_humidity = 75.0;
    Stanag4355::cfg_wind_dir = 0.0;
    Stanag4355::cfg_wind_spd = 2.5;

    map_e_max=0;
    map_e_min=0;
    map_n_max=0;
    map_n_min=0;

    gb_e = 0.0;
    gb_n = 0.0;
    gb_alt = 0.0;

    cob_expected_qty = 0;
    cob_current_index = 0;

    temp_dir = 0.0;
    temp_dist = 0.0;
    temp_iv = 0.0;

    base_piece_index = 0;

    guns.clear();
    gun_dirs.clear();

    ammo_proj_prop="";
    ammo_proj_lot="";
    ammo_proj_wt=0;

    mission_type="";

    reg_dist=0;
    reg_def=0;
    df_corr=0;
    time_reg_correction=0;

    shift_lr=0;
    shift_ad=0;
    shift_ud=0;

    inst_last_dir = 0.0;
    inst_new_dir = 0.0;
    inst_angle_t = 0.0;

    mission_active=false;
    last_solution="";
    ffe_mode=false;
    ammo_input_active=false;
    sheaf_mode="CONV";

    mission_log.clear();
    last_inputs.clear();

    fuze_time_mode=false;
    hob=0;
    time_reg_fuze_temporary = false;
    ud_corr=0;
    ud_active=false;

    manual_chg_enabled = false;
    manual_chg_value = "";
    chg_allowed = false;
    chg_edit_mode = false;
    chg_wait_value = false;
    base_piece_index = 0;
    reg_data_available = false;
    last_knpt = 0;
    last_proj = "";
    last_lot = "";
    last_fuze_tia = false;
    last_reg_rg = 0.0;
    last_reg_def = 0.0;

    fm1_tgt_method = 0;
    fm1_temp_knpt = 0;
    fm1_temp_e = 0.0;
    fm1_temp_n = 0.0;
    fm1_temp_alt = 0.0;
    fm1_base_def_reverse = false;
    active_fm1_def_reverse = false;
    active_fm1_transport_qe_shape = false;

    fm1_grid_clean_fire_pending = false;
    fm1_grid_reg_dist_backup = 0.0;
    fm1_grid_reg_def_backup = 0.0;
    fm1_grid_df_corr_backup = 0.0;
    fm1_grid_time_reg_correction_backup = 0.0;
    fm1_grid_ud_active_backup = false;
    fm1_grid_ud_corr_backup = 0.0;

    fm1_from_knpt = 0;

    fm1_tr_az = 0.0;

    fm1_tr_lr = 'N';
    fm1_tr_lr_val = 0.0;

    fm1_tr_ad = 'N';
    fm1_tr_ad_val = 0.0;

    fm1_tr_ud = 'N';
    fm1_tr_ud_val = 0.0;

    fm1_pol_az = 0.0;
    fm1_pol_dist = 0.0;

    fm1_pol_ud = 'N';
    fm1_pol_ud_val = 0.0;

    az_lay = 0.0;

    def_base = 3200;

    bool loaded = false;

    std::ifstream f1("tables.csv");
    if(f1.good())
    {
        loadTablesFromCSV("tables.csv");
        loaded = true;
    }

    if(!loaded)
    {
        std::ifstream f2("backend/tables.csv");
        if(f2.good())
        {
            loadTablesFromCSV("backend/tables.csv");
            loaded = true;
        }
    }

    if(!loaded)
    {
        std::string fullPath = "";

#ifdef _WIN32
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);

        std::string exePath(buffer);
        size_t pos = exePath.find_last_of("\\/");

        std::string dir = exePath.substr(0, pos);
        fullPath = dir + "\\tables.csv";
#else
        char buffer[4096];
        ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

        if(len > 0)
        {
            buffer[len] = '\0';

            std::string exePath(buffer);
            size_t pos = exePath.find_last_of("/");

            std::string dir = exePath.substr(0, pos);
            fullPath = dir + "/tables.csv";
        }
#endif

        if(!fullPath.empty())
        {
            std::ifstream f3(fullPath);
            if(f3.good())
            {
                loadTablesFromCSV(fullPath);
                loaded = true;
            }
        }
    }
    if(!loaded)
    {
        std::cout << "ERROR: tables.csv no encontrado\n";
    }

    return "DATA CLEARED\nMAIN (? 1 3 4 5 7 X *)";

}