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
    // 🔥 eliminar caracteres basura (ENTER, TAB, etc)
    for(char& c : s)
    {
        if(c == '\n' || c == '\r' || c == '\t')
            c = ' ';
    }

    // trim derecha
    while(!s.empty() && s.back() == ' ')
        s.pop_back();

    // trim izquierda
    size_t i = 0;
    while(i < s.size() && s[i] == ' ')
        ++i;

    if(i >= s.size())
        return "";

    return s.substr(i);
}

//////////////////////////////////////////////////
// TABLAS
//////////////////////////////////////////////////

// ======================= STRUCTS =======================

struct Row
{
    double d;
    double qe;
    double tof;
    double drift;
};

struct AmmoKey
{
    std::string artillery;
    std::string proj;
    int chg;

    bool operator<(const AmmoKey& o) const
    {
        return std::tie(artillery, proj, chg) <
               std::tie(o.artillery, o.proj, o.chg);
    }
};

// ======================= STORAGE =======================

static std::map<AmmoKey, std::vector<Row>> firingTables;

// ======================= CSV LOADER =======================

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

    std::getline(file, line); // header

    while(std::getline(file, line))
    {
        if(line.empty()) continue;

        std::stringstream ss(line);
        std::string token;

        AmmoKey key;
        Row row;

        std::getline(ss, token, ','); key.artillery = trim(token);
        std::getline(ss, token, ','); key.proj = trim(token);
        std::getline(ss, token, ','); key.chg = std::stoi(trim(token));

        std::getline(ss, token, ','); row.d = std::stod(trim(token));
        std::getline(ss, token, ','); row.qe = std::stod(trim(token));
        std::getline(ss, token, ','); row.tof = std::stod(trim(token));
        std::getline(ss, token, ','); row.drift = std::stod(trim(token));
        firingTables[key].push_back(row);
    }

    for(auto& it : firingTables)
    {
        std::sort(it.second.begin(), it.second.end(),
            [](const Row& a,const Row& b){ return a.d < b.d; });
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
// 0 BASE
// 1 REG
// 2 TIME REG
// 3 SHIFT
// 4 PMI

// 🔥 COB DOCTRINAL VARIABLES
static double gb_e = 0.0;
static double gb_n = 0.0;
static double gb_alt = 0.0;
static double az_lay = 0.0;

static int cob_expected_qty = 0;
static int cob_current_index = 0;

static double temp_dir = 0.0;
static double temp_dist = 0.0;
static double temp_iv = 0.0;

// 🔥 BASE PIECE DOCTRINAL (FALTABA)
static int base_piece_index = 0;

// 🔥 OPEN DOCTRINAL
static double sheaf_width = 60.0; // ancho total en mils

// 🔥 DOCTRINAL A
static bool all_guns_command = false; 

// ==========================
// DEFINICIONES GLOBALES PARA PDF
// ==========================

double map_e_max = 0;
double map_e_min = 0;
double map_n_max = 0;
double map_n_min = 0;
double map_gz = 0;
std::string map_spher = "";

//////////////////////////////////////////////////
// PDF GENERATOR (NUEVO - NO TOCAR NADA EXISTENTE)
//////////////////////////////////////////////////

// ==========================
// VARIABLES GLOBALES NECESARIAS PARA PDF
// ==========================

static int def_base = 3200;
static std::string artillery_type = "155";

static std::string ammo_proj_prop = "";
static std::string ammo_proj_lot = "";

static double obs_alt = 0;
static double obs_e = 0;
static double obs_n = 0;

static std::vector<std::string> main_inputs;

static std::vector<std::string> last_inputs;

std::string escapePDF(const std::string& text)
{
    std::string result;
    for(char c : text)
    {
        if(c=='(' || c==')' || c=='\\')
            result += '\\';
        result += c;
    }
    return result;
}

struct ShotLog
{
    std::string label;
    std::string inputs;
    std::string fire;
    std::string timestamp;
};

static std::vector<ShotLog> mission_log;

void saveReportPDF(const std::vector<std::string>& lines, int mission_counter)
{
    struct ShotRow
    {
        std::string cob;
        std::string dist;
        std::string az;
        std::string def;
        std::string chg;
        std::string qe;
        std::string tof;
        std::string fuze;
    };

    auto startsWith = [](const std::string& s, const std::string& prefix) -> bool
    {
        return s.rfind(prefix, 0) == 0;
    };

    auto trim = [](const std::string& s) -> std::string
    {
        size_t start = 0;
        while(start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
            ++start;

        size_t end = s.size();
        while(end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
            --end;

        return s.substr(start, end - start);
    };

    auto pad = [](const std::string& s, int width, bool rightAlign = false) -> std::string
    {
        if((int)s.size() >= width)
            return s.substr(0, width);

        std::string spaces(width - s.size(), ' ');
        return rightAlign ? (spaces + s) : (s + spaces);
    };

    auto makeSeparator = []() -> std::string
    {
        return "------------------------------------------------------------";
    };

    auto makeTableHeader = [&]() -> std::string
    {
        return "COB | DIST   | AZ   | DEF   | CHG | QE      | TOF   | FUZE        ";
    };

    auto makeTableRule = [&]() -> std::string
    {
       return "----+--------+------+-------+-----+---------+-------+-------------";
    };

    auto makeRowLine = [&](const ShotRow& r) -> std::string
    {
        return pad(r.cob, 3, true) + " | " +
               pad(r.dist, 6, true) + " | " +
               pad(r.az,   4, true) + " | " +
               pad(r.def,  5, true) + " | " +
               pad(r.chg,  3, true) + " | " +
               pad(r.qe,   7, true) + " | " +
               pad(r.tof,  5, true) + " | " +
               pad(r.fuze, 11, false);
    };

    std::string fecha;
    std::string tgt_alt = "0";
    std::string tgt_n   = "0";
    std::string tgt_e   = "0";

    std::vector<ShotRow> base_rows;
    std::vector<ShotRow> adjusted_rows;

    int current_section = 0; // 0 ninguno, 1 base, 2 adjusted
    bool shot_open = false;
    ShotRow current_row;

    auto flushShot = [&]()
    {
        if(!shot_open)
            return;

        // 🔥 FILTRAR SOLO PIEZA BASE
        int piece_num = -1;

        try
        {
            piece_num = std::stoi(current_row.cob);
        }
        catch(...)
        {
            piece_num = -1;
        }

        if(piece_num == base_piece_index + 1)
        {
            if(current_section == 1)
            {
                base_rows.push_back(current_row);
            }
            else if(current_section == 2)
            {
                adjusted_rows.push_back(current_row);
            }
        }

        current_row = ShotRow();
        shot_open = false;
    };

    // ==============================
    // PARSEAR lines ACTUALES
    // ==============================
    for(size_t i = 0; i < lines.size(); i++)
    {
        std::string line = trim(lines[i]);
        if(line.empty())
            continue;

        if(startsWith(line, "BASE FIRE"))
        {
            flushShot();
            current_section = 1;
            continue;
        }

        if(startsWith(line, "REGISTRATION") ||
        startsWith(line, "TIME REG") ||
        startsWith(line, "SHIFT (FM3)") ||
        startsWith(line, "PMI (FM4)"))
        {
            flushShot();
            current_section = 2; 
            continue;
        }

        if(startsWith(line, "FECHA:"))
        {
            fecha = line.substr(6);
            fecha = trim(fecha);
            continue;
        }

        if(startsWith(line, "ALT "))
        {
            tgt_alt = trim(line.substr(4));
            continue;
        }

        if(startsWith(line, "N "))
        {
            tgt_n = trim(line.substr(2));
            continue;
        }

        if(startsWith(line, "E "))
        {
            tgt_e = trim(line.substr(2));
            continue;
        }

        if(startsWith(line, "----- PIECE "))
        {
            flushShot();

            current_row = ShotRow();
            shot_open = true;

            std::string tmp = line.substr(13); // correcto: empieza en el número
            size_t spacePos = tmp.find(' ');
            if(spacePos != std::string::npos)
                current_row.cob = trim(tmp.substr(0, spacePos));
            else
                current_row.cob = trim(tmp);

            continue;
        }

        if(!shot_open)
            continue;

        if(startsWith(line, "DIST "))
        {
            std::string val = trim(line.substr(5));

            try
            {
                if(std::stod(val) <= 0)
                {
                    shot_open = false;
                    continue;
                }
            }
            catch(...)
            {
                shot_open = false;
                continue;
            }

            current_row.dist = val;
            continue;
        }

        if(startsWith(line, "AZ "))
        {
            current_row.az = trim(line.substr(3));
            continue;
        }

        if(startsWith(line, "DEF "))
        {
            current_row.def = trim(line.substr(4));
            continue;
        }

        if(startsWith(line, "CHG "))
        {
            current_row.chg = trim(line.substr(4));
            continue;
        }

        if(startsWith(line, "QE "))
        {
            std::string val = trim(line.substr(3));

            try
            {
                double qe_val = std::stod(val);

                if(qe_val < -1000 || qe_val > 2000)
                {
                    shot_open = false;
                    continue;
                }
            }
            catch(...)
            {
                shot_open = false;
                continue;
            }

            current_row.qe = val;
            continue;
        }

        if(startsWith(line, "TOF "))
        {
            current_row.tof = trim(line.substr(4));
            continue;
        }

        if(startsWith(line, "FUZE "))
        {
            current_row.fuze = trim(line.substr(5));
            continue;
        }
    }

    flushShot();

    // ==============================
    // ARMAR CUERPO DEL REPORTE
    // ==============================
    std::vector<std::string> body_lines;

    // ==============================
    // 🔥 CONTEXTO DOCTRINAL COMPLETO
    // ==============================

    body_lines.push_back("FECHA: " + fecha);
    body_lines.push_back("");

    // ==============================
    // MAP MODEL
    // ==============================
    body_lines.push_back("MAP MODEL");
    body_lines.push_back(makeSeparator());

    std::stringstream mm;

    mm << "MAX E: " << std::setw(8) << (int)map_e_max
    << "   MIN E: " << std::setw(8) << (int)map_e_min;
    body_lines.push_back(mm.str());

    mm.str(""); mm.clear();
    mm << "MAX N: " << std::setw(8) << (int)map_n_max
    << "   MIN N: " << std::setw(8) << (int)map_n_min;
    body_lines.push_back(mm.str());

    mm.str(""); mm.clear();
    mm << "GZ: " << std::setw(5) << (int)map_gz
    << "   SPHER: " << map_spher;
    body_lines.push_back(mm.str());

    body_lines.push_back("");

    // ==============================
    // AMMO FILE
    // ==============================
    body_lines.push_back("AMMO FILE");
    body_lines.push_back(makeSeparator());

    std::stringstream af;
    af << "ART: " << artillery_type
    << "  PROJ: " << ammo_proj_prop
    << "  LOT: " << ammo_proj_lot;

    body_lines.push_back(af.str());
    body_lines.push_back("");

    // ==============================
    // COB / FIRE DATA
    // ==============================
    body_lines.push_back("FIRE DATA");
    body_lines.push_back(makeSeparator());

    std::stringstream fd;
    fd << "GB E: " << (int)gb_e
    << "  GB N: " << (int)gb_n
    << "  ALT: " << (int)gb_alt;
    body_lines.push_back(fd.str());

    fd.str(""); fd.clear();
    fd << "AZ LAY: " << (int)az_lay
    << "  REF DEF: " << def_base;
    body_lines.push_back(fd.str());

    fd.str(""); fd.clear();
    fd << "BASE PIECE: " << (base_piece_index + 1);
    body_lines.push_back(fd.str());

    body_lines.push_back("");

    // ==============================
    // OBSERVER
    // ==============================
    body_lines.push_back("OBSERVER");
    body_lines.push_back(makeSeparator());

    std::stringstream obs_ss;
    obs_ss << "ALT: " << (int)obs_alt
        << "  E: " << (int)obs_e
        << "  N: " << (int)obs_n;

    body_lines.push_back(obs_ss.str());
    body_lines.push_back("");

    // ==============================
    // TARGET
    // ==============================
    body_lines.push_back("TARGET DATA");
    body_lines.push_back(makeSeparator());

    body_lines.push_back("ALT: " + tgt_alt + "     N: " + tgt_n + "     E: " + tgt_e);
    body_lines.push_back("");

    // ==============================
    // INPUTS FM (LOG REAL)
    // ==============================
    body_lines.push_back("MAIN DATA");
    body_lines.push_back("------------------------------------------------------------");

    for(const auto& line : main_inputs)
    {
        body_lines.push_back(line);
    }

    body_lines.push_back("");
    body_lines.push_back("FM INPUT LOG");
    body_lines.push_back(makeSeparator());

    for(const auto& shot : mission_log)
    {
        if(!shot.inputs.empty())
        {
            std::stringstream ss(shot.inputs);
            std::string line;

            while(std::getline(ss, line))
            {
                if(!line.empty())
                    body_lines.push_back(line);
            }
        }
    }

    body_lines.push_back("");

    // ======================================
    // 🔥 RESULTADO FINAL (SOLO PIEZA BASE)
    // ======================================

std::string final_fire;


// ==============================
// 🔥 INPUTS DEL ÚLTIMO DISPARO
// ==============================
if(!mission_log.empty())
{
    for(const auto& shot : mission_log)
    {
        body_lines.push_back("FM INPUT");
        body_lines.push_back(makeSeparator());

        std::stringstream ss_in(shot.inputs);
        std::string line;

        while(std::getline(ss_in, line))
        {
            line = trim(line);
            if(!line.empty())
                body_lines.push_back(line);
        }

        body_lines.push_back("");

        // 🔥 también meter su fire aquí si quieres histórico
    }
}

// ==============================
// 🔥 RESULTADO PIEZA BASE
// ==============================
if(!mission_log.empty())
{
    final_fire = mission_log.back().fire;

    std::stringstream ss(final_fire);
    std::string line;

    bool capture = false;

    while(std::getline(ss, line))
    {
        line = trim(line);

        if(line.find("----- PIECE") != std::string::npos)
        {
            int piece_num = -1;
            sscanf(line.c_str(), "----- PIECE %d -----", &piece_num);

            if(piece_num == base_piece_index + 1)
            {
                capture = true;
                body_lines.push_back(line);
            }
            else
            {
                capture = false;
            }

            continue;
        }

        if(capture && !line.empty())
        {
            body_lines.push_back(line);
        }
    }
}
else
{
    body_lines.push_back("NO FIRE DATA AVAILABLE");
}

    body_lines.push_back(makeSeparator());
    body_lines.push_back("MISSION CLOSED");
    body_lines.push_back(makeSeparator());

    // ==============================
    // PAGINADO
    // ==============================
    std::vector<std::string> pages;
    std::stringstream page_stream;

    const int margin_top = 760;
    const int margin_bottom = 50;
    const int line_height = 12;

    int page_number = 1;
    int y = 650;

    auto writeHeader = [&](std::stringstream& p, int pageNo)
    {
        // ============================
        // FUERZAS ARMADAS DE HONDURAS
        // ============================

        p << "BT\n/F1 12 Tf\n120 770 Td\n(FUERZAS ARMADAS DE HONDURAS) Tj\nET\n";

        // Línea superior
        p << "BT\n/F1 10 Tf\n50 758 Td\n(============================================================) Tj\nET\n";

        // ============================
        // BANDERA (ESTRELLAS)
        // ============================

        p << "BT\n/F1 10 Tf\n50 745 Td\n(~~~~~~~~~~~~~~~~~~~~~~   *       *   ~~~~~~~~~~~~~~~~~~~~~~) Tj\nET\n";
        p << "BT\n/F1 10 Tf\n50 732 Td\n(~~~~~~~~~~~~~~~~~~~~~~       *       ~~~~~~~~~~~~~~~~~~~~~~) Tj\nET\n";
        p << "BT\n/F1 10 Tf\n50 719 Td\n(~~~~~~~~~~~~~~~~~~~~~~   *       *   ~~~~~~~~~~~~~~~~~~~~~~) Tj\nET\n";

        // Línea media
        p << "BT\n/F1 10 Tf\n50 706 Td\n(============================================================) Tj\nET\n";

        // ARTILLERIA
        p << "BT\n/F1 12 Tf\n170 692 Td\n(ARTILLERIA AD GLORIUM) Tj\nET\n";

        // Línea inferior
        p << "BT\n/F1 10 Tf\n50 678 Td\n(============================================================) Tj\nET\n";

        // ============================
        // TITULO
        // ============================

        std::stringstream mission_ss;
        mission_ss << "REPORTE DE MISION #"
                << std::setw(2) << std::setfill('0') << mission_counter;

        p << "BT\n/F1 11 Tf\n140 660 Td\n(" << escapePDF(mission_ss.str()) << ") Tj\nET\n";

        // ============================
        // PAGINA
        // ============================

        std::stringstream page_ss;
        page_ss << "PAGE " << pageNo;

        p << "BT\n/F1 10 Tf\n450 660 Td\n(" << escapePDF(page_ss.str()) << ") Tj\nET\n";
    };

    writeHeader(page_stream, page_number);

    for(size_t i = 0; i < body_lines.size(); i++)
    {
        if(y < margin_bottom)
        {
            pages.push_back(page_stream.str());
            page_stream.str("");
            page_stream.clear();

            page_number++;
            writeHeader(page_stream, page_number);
            y = 660;
        }

        page_stream << "BT\n";
        page_stream << "/F1 9 Tf\n";
        page_stream << "50 " << y << " Td\n";
        page_stream << "(" << escapePDF(body_lines[i]) << ") Tj\n";
        page_stream << "ET\n";

        y -= line_height;
    }

    if(!page_stream.str().empty())
        pages.push_back(page_stream.str());

    // ==============================
    // ESCRIBIR PDF
    // ==============================
    std::stringstream filename;
    filename << "Reporte_Mision_"
            << std::setw(2) << std::setfill('0') << mission_counter
            << ".pdf";
    std::cout << "GUARDANDO EN: " << filename.str() << std::endl;
    
    std::ofstream pdf(filename.str(), std::ios::binary);
    
    if(!pdf.is_open())
    {
        std::cout << "ERROR: NO SE PUDO CREAR PDF\n";
        return;
    }

    pdf << "%PDF-1.4\n";

    const int page_count = static_cast<int>(pages.size());
    const int font_obj = 3 + (page_count * 2);

    std::vector<long> offsets(font_obj + 1, 0);

    // 1 Catalog
    offsets[1] = static_cast<long>(pdf.tellp());
    pdf << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    // 2 Pages
    offsets[2] = static_cast<long>(pdf.tellp());
    pdf << "2 0 obj\n<< /Type /Pages /Kids [";
    for(int i = 0; i < page_count; i++)
    {
        int page_obj = 3 + (i * 2);
        pdf << page_obj << " 0 R ";
    }
    pdf << "] /Count " << page_count << " >>\nendobj\n";

    // Pages + Contents
    for(int i = 0; i < page_count; i++)
    {
        int page_obj = 3 + (i * 2);
        int content_obj = page_obj + 1;

        offsets[page_obj] = static_cast<long>(pdf.tellp());
        pdf << page_obj << " 0 obj\n";
        pdf << "<< /Type /Page /Parent 2 0 R\n";
        pdf << "/MediaBox [0 0 612 792]\n";
        pdf << "/Contents " << content_obj << " 0 R\n";
        pdf << "/Resources << /Font << /F1 " << font_obj << " 0 R >> >> >>\n";
        pdf << "endobj\n";

        offsets[content_obj] = static_cast<long>(pdf.tellp());
        pdf << content_obj << " 0 obj\n";
        pdf << "<< /Length " << pages[i].size() << " >>\n";
        pdf << "stream\n";
        pdf << pages[i];
        pdf << "endstream\n";
        pdf << "endobj\n";
    }

    // Font
    offsets[font_obj] = static_cast<long>(pdf.tellp());
    pdf << font_obj << " 0 obj\n";
    pdf << "<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\n";
    pdf << "endobj\n";

    long xref_pos = static_cast<long>(pdf.tellp());

    pdf << "xref\n0 " << (font_obj + 1) << "\n";
    pdf << "0000000000 65535 f \n";
    for(int i = 1; i <= font_obj; i++)
    {
        pdf << std::setw(10) << std::setfill('0') << offsets[i]
            << " 00000 n \n";
    }

    pdf << "trailer\n<< /Size " << (font_obj + 1) << " /Root 1 0 R >>\n";
    pdf << "startxref\n" << xref_pos << "\n%%EOF";

    pdf.close();
}


//////////////////////////////////////////////////
// DR EVIL
//////////////////////////////////////////////////

std::string drPrefix(const std::string& menu,const std::string& text)
{
    if(menu=="MAIN" || menu=="FM")
        return "DR EVIL\n"+text;
    return text;
}

//////////////////////////////////////////////////
// ESTADO GLOBAL
//////////////////////////////////////////////////

static bool mission_active = false;
// 🔥 CAMBIO DE CARGA DOCTRINAL
static bool manual_chg_enabled = false;
static int manual_chg_value = 0;
static bool chg_allowed = false;
static bool chg_edit_mode = false;
static bool chg_wait_value = false;
static std::string last_solution = "";

static std::vector<double> gun_dirs;
static int mission_counter = 1;
static bool ffe_mode = false;
static bool ammo_input_active = false;
bool initializing = false;


// SHEAF
static std::string sheaf_mode = "CONV";

//////////////////////////////////////////////////
// FM1 VARIABLES
//////////////////////////////////////////////////

static int fm1_tgt_method = 0;   // 1=GRID, 2=TRANSPORT, 3=POLAR
static int fm1_temp_knpt = 0;

static double fm1_temp_e = 0.0;
static double fm1_temp_n = 0.0;
static double fm1_temp_alt = 0.0;
static bool fm1_base_def_reverse = false;
static bool active_fm1_def_reverse = false;
static bool active_fm1_transport_qe_shape = false;

// FM1 GRID / TGT1 CLEAN FIRE
static bool fm1_grid_clean_fire_pending = false;
static double fm1_grid_reg_dist_backup = 0.0;
static double fm1_grid_reg_def_backup = 0.0;
static double fm1_grid_df_corr_backup = 0.0;
static double fm1_grid_time_reg_correction_backup = 0.0;
static bool fm1_grid_ud_active_backup = false;
static double fm1_grid_ud_corr_backup = 0.0;

//////////////////////////////////////////////////
// FM1 POLAR VARIABLES
//////////////////////////////////////////////////

static double fm1_pol_az = 0.0;
static double fm1_pol_dist = 0.0;

static char fm1_pol_ud = 'N';   // S o B
static double fm1_pol_ud_val = 0.0;

//////////////////////////////////////////////////
// FM1 TRANSPORT VARIABLES
//////////////////////////////////////////////////

static int fm1_from_knpt = 0;

static double fm1_tr_az = 0.0;

static char fm1_tr_lr = 'N';   // I o D
static double fm1_tr_lr_val = 0.0;

static char fm1_tr_ad = 'N';   // + o -
static double fm1_tr_ad_val = 0.0;

static char fm1_tr_ud = 'N';   // S o B
static double fm1_tr_ud_val = 0.0;

//////////////////////////////////////////////////
// VARIABLES REG / CORR
//////////////////////////////////////////////////

bool boot_mode = true;
static double reg_dist = 0.0;
static double reg_def = 0.0;
static double df_corr = 0.0;
static double time_reg_correction = 0.0;

//////////////////////////////////////////////////
// SHIFT VARIABLES
//////////////////////////////////////////////////

static double shift_lr = 0.0;
static double shift_ad = 0.0;
static double shift_ud = 0.0;
static double last_dist_solution = 0.0;
static double last_def_solution = 0.0;
static double last_qe_solution = 0.0;

//////////////////////////////////////////////////
// INSTANTANEOUS PDA / SHIFT CORR VARIABLES
//////////////////////////////////////////////////

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

// SHIFT DOCTRINAL
static std::string shift_prev_dir = "N";
static double shift_prev_lr = 0.0;
static double shift_prev_ad = 0.0;
static double shift_prev_ud = 0.0;

static std::string shift_new_dir = "N";
static double shift_angle = 0.0;
// 🔥 REG MEMORY (FM2)
static bool reg_data_available = false;

static int last_knpt = 0;
static std::string last_proj = "";
static std::string last_lot = "";
static bool last_fuze_tia = false;
static double last_reg_rg = 0.0;
static double last_reg_def = 0.0;
static bool time_reg_fuze_temporary = false;

//////////////////////////////////////////////////
// INTERPOLACION
//////////////////////////////////////////////////

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

            qe = a.qe + f * (b.qe - a.qe);
            tof = a.tof + f * (b.tof - a.tof);
            drift = a.drift + f * (b.drift - a.drift);

            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////
// SOLVER
//////////////////////////////////////////////////

static bool solveAuto(const std::string& proj,double dist,
                     int& chg,double& qe,double& tof,double& drift)
{
    std::cout << "BUSCANDO → ART=" << artillery_type
          << " PROJ=" << proj
          << " DIST=" << dist << "\n";

    bool found = false;
    double bestExcess = 1e9;

    for(auto it = firingTables.begin(); it != firingTables.end(); ++it)
    {
        const AmmoKey& key = it->first;
        const std::vector<Row>& table = it->second;

        // 🔥 FILTRO POR ARTILLERY
        if(normStr(key.artillery) != normStr(artillery_type))
            continue;

        if(normStr(key.proj) != normStr(proj))
            continue;

        double q,t,d;

        if(!interp(table, dist, q, t, d))
            continue;

        double candidate_qe = q;
        double candidate_tof = t;
        double candidate_drift = d;
        int candidate_chg = key.chg;

        // Prioridad doctrinal:
        // 1. La carga más baja que resuelva la distancia
        // 2. Si hay empate, menor QE
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

static bool solveByCharge(const std::string& proj,double dist,int requested_chg,
                         int& chg,double& qe,double& tof,double& drift)
{
    for(auto it = firingTables.begin(); it != firingTables.end(); ++it)
    {
        const AmmoKey& key = it->first;
        const std::vector<Row>& table = it->second;

        if(normStr(key.artillery) != normStr(artillery_type))
            continue;

        if(normStr(key.proj) != normStr(proj))
            continue;

        if(key.chg != requested_chg)
            continue;

        if(!interp(table, dist, qe, tof, drift))
            continue;

        chg = key.chg;
        return true;
    }

    chg = 0;
    qe = 0;
    tof = 0;
    drift = 0;
    return false;
}


// 🔥 Compatibilidad con el resto del sistema
static bool solve(const std::string& proj,double dist,int& chg,double& qe,double& tof,double& drift)
{
    if(manual_chg_enabled)
        return solveByCharge(proj,dist,manual_chg_value,chg,qe,tof,drift);

    return solveAuto(proj,dist,chg,qe,tof,drift);
}

//////////////////////////////////////////////////
// SITE
//////////////////////////////////////////////////

static double computeSite(double iv,double dist)
{
    if(dist==0) return 0;
    return (iv/dist)*1000.0*1.0186;
}

//////////////////////////////////////////////////
// PARSER U/D
//////////////////////////////////////////////////

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
            // sigue abajo
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

//////////////////////////////////////////////////
// TIME STAMP
//////////////////////////////////////////////////

    std::string getCurrentTime()
    {
        std::time_t now = std::time(nullptr);
        std::tm* local = std::localtime(&now);

        std::stringstream ss;
        ss << std::put_time(local, "%Y-%m-%d %H:%M:%S");

        return ss.str();
    }

//////////////////////////////////////////////////
// EXECUTE
//////////////////////////////////////////////////

std::string BasicEngine::execute(const std::string& input)
{
    std::string cmd = input;

    // 🔥 BOOT SEQUENCE (SANTA BARBARA)
    if(boot_mode)
    {
        std::string v = normStr(cmd);

        // Primera pantalla
        if(v.empty())
        {
            return "SANTA BARBARA\n>";
        }

        // Usuario escribe RUNBUCS
        if(v == "RUNBUCS")
        {
            boot_mode = false;
            initializing = true;

            return "INITIALIZE";
        }

        // Cualquier otro comando
        return "SANTA BARBARA\n>";
    }

    // TRANSICION AUTOMATICA DESPUES DE INITIALIZE
    //
    // IMPORTANTE:
    // El engine NO debe dormir ni congelar la aplicacion aqui.
    // La espera de 2 segundos la hace la interfaz.
    //
    // Cuando la interfaz mande una entrada vacia despues del timer,
    // este bloque cambia el sistema a MAIN.
    if(initializing)
    {
        initializing = false;
        current_menu = "MAIN";

        return "DR EVIL\nMAIN (? 1 3 4 5 7 X *)";
    }

    // 🔽 aquí sigue TODO tu sistema normal (NO TOCAR)

    auto renderFire = [&](bool useFFE)->std::string
{
    std::stringstream out;

    double base_e = guns[base_piece_index].e;
    double base_n = guns[base_piece_index].n;
    double base_alt = guns[base_piece_index].alt;

// ===============================
// 🔥 SHIFT VECTORIAL REAL (TARGET AJUSTADO)
// ===============================
    double adj_tgt_e = tgt_e;
    double adj_tgt_n = tgt_n;

    if(shift_lr != 0 || shift_ad != 0)
    {
        double dir_rad = shift_angle * (2.0 * PI / 6400.0);

        // Vector A/D real: sobre la linea pieza-target.
        double ad_e = std::sin(dir_rad);
        double ad_n = std::cos(dir_rad);

        // Vector L/R real: perpendicular a la linea pieza-target.
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
        // 🔥 CUANTIZACIÓN A 2 MILS (MIRAS ESTÁNDAR)
        double mils = std::floor((mils_raw + 1.5) / 2.0) * 2.0;

        int chg = 0;
        double qe = 0, tof = 0, drift = 0;

        double dist = dist_geom + reg_dist;

        // 🔥 PRIMERO resolver balística
        bool solved = solve(ammo_proj_prop, dist, chg, qe, tof, drift);

        // =====================================================
        // 🔥 MODULACIÓN QE SOLO PARA FM1 TRANSPORT / TGT2
        // =====================================================
        if(active_fm1_transport_qe_shape &&
           artillery_type == "155" &&
           (ammo_proj_prop == "HEA" || ammo_proj_prop == "M4A2") &&
           chg == 6)
        {
            double mid_factor = 0.0;

            if(dist >= 9900.0 && dist < 10400.0)
            {
                auto smooth = [](double edge0, double edge1, double x) -> double
                {
                    if(edge0 == edge1)
                        return 0.0;

                    double t = (x - edge0) / (edge1 - edge0);

                    if(t < 0.0) t = 0.0;
                    if(t > 1.0) t = 1.0;

                    return t * t * (3.0 - 2.0 * t);
                };

                if(dist < 10050.0)
                {
                    mid_factor = smooth(9900.0, 10050.0, dist);
                }
                else if(dist <= 10300.0)
                {
                    mid_factor = 1.0;
                }
                else
                {
                    mid_factor = 1.0 - smooth(10300.0, 10400.0, dist);
                }
            }

            qe *= 1.0 - (0.0284 * mid_factor);
        }
        
        // ================================
        // 🔥 DRIFT SINTÉTICO (SI TABLA = 0)
        // ================================
        if(std::abs(drift) < 0.001)
        {
            drift = 0.00038 * dist;  // factor realista artillería 155
        }

        // 🔥 AHORA sí calcular DEF correctamente

        bool use_fm1_reverse = fm1_base_def_reverse || active_fm1_def_reverse;

        double def = 0.0;

        if(use_fm1_reverse)
            def = def_base - (mils - az_lay);
        else
            def = def_base + (mils - az_lay);

        if(!use_fm1_reverse)
            def += reg_def + df_corr;

        // ================================
        // 🔥 MÓDULO DEF FÍSICO MEJORADO
        // ================================

        // ================================
        // 🔥 DEFLEXIÓN ACUMULADA 
        // ================================

        double drift_accum = drift * (tof / 25.0);
        double jump_h = 0.3;

        if(use_fm1_reverse)
        {
            drift_accum = 0.0;
            jump_h = 0.0;
        }

        if((int)i == base_piece_index)
        {
            std::cout << "\n[DEF DEBUG BASE PIECE]\n";
            std::cout << "MENU=" << current_menu << "\n";
            std::cout << "AZ=" << mils << "\n";
            std::cout << "AZ_LAY=" << az_lay << "\n";
            std::cout << "REF_DEF=" << def_base << "\n";
            std::cout << "DEF_BEFORE_CORR=" << (def_base + (mils - az_lay)) << "\n";
            std::cout << "REG_DEF=" << reg_def << "\n";
            std::cout << "DF_CORR=" << df_corr << "\n";
            std::cout << "DRIFT=" << drift << "\n";
            std::cout << "TOF=" << tof << "\n";
            std::cout << "DRIFT_ACCUM=" << drift_accum << "\n";
            std::cout << "JUMP_H=" << jump_h << "\n";
            std::cout << "DEF_BEFORE_ROUND=" << (def + drift_accum + jump_h) << "\n";
            std::cout << "[/DEF DEBUG BASE PIECE]\n\n";
        }

        def += drift_accum;
        def += jump_h;
        // 🔥 REDONDEO HP71
        def = std::round(def);

        // 🔥 normalizar AL FINAL
        while(def < 0) def += 6400;
        while(def >= 6400) def -= 6400;

        if(manual_chg_enabled)
            chg = manual_chg_value;

        double iv = tgt_alt - guns[i].alt + shift_ud;
        double site = computeSite(iv,dist);

        double qe_final = qe + site + time_reg_correction;
        if(ud_active)
            qe_final += (ud_corr*0.05);

// ===============================
// 🔥 APLICAR OPEN (FALTABA)
// ===============================
        if(sheaf_mode == "OPEN" && guns.size() > 1)
        {
            int total_guns = (int)guns.size();

            double center = (total_guns - 1) / 2.0;
            double spacing = sheaf_width / (total_guns - 1);

            double offset = (i - center) * spacing;

            def += offset;
        }

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

        // 🔥 NORMALIZAR DESPUÉS DEL AJUSTE FINAL
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
        // 🔥 GUARDAR ÚLTIMA SOLUCIÓN BASE (PARA REG)

        // usar la PIEZA BASE (correcto doctrinalmente)
        int i = base_piece_index;

        double dx = adj_tgt_e - guns[i].e;
        double dy = adj_tgt_n - guns[i].n;

        double dist_geom = std::sqrt(dx*dx + dy*dy);

        double az = std::atan2(dx,dy);
        if(az < 0) az += 2*PI;

        double mils_raw = az * (6400 / (2 * PI));
        double mils = std::floor((mils_raw + 1.5) / 2.0) * 2.0;

        double dist = dist_geom + reg_dist;

        // resolver balística
        int chg_tmp = 0;
        double qe_tmp = 0, tof_tmp = 0, drift_tmp = 0;

        bool solved_ref = solve(ammo_proj_prop, dist, chg_tmp, qe_tmp, tof_tmp, drift_tmp);

        if(std::abs(drift_tmp) < 0.001)
        {
            drift_tmp = 0.00038 * dist;
        }

        if(solved_ref)
        {
        bool use_fm1_reverse_ref = fm1_base_def_reverse || active_fm1_def_reverse;

        double def_ref = 0.0;

        if(use_fm1_reverse_ref)
            def_ref = def_base - (mils - az_lay);
        else
            def_ref = def_base + (mils - az_lay);

        if(!use_fm1_reverse_ref)
            def_ref += reg_def + df_corr;

            double drift_accum = drift_tmp * (tof_tmp / 25.0);
            double jump_h = 0.3;

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

//////////////////////////////////////////////////
// ANGLE T - ANGULO ENTRE DIR OBSERVADO Y LINEA DE TIRO
//////////////////////////////////////////////////

auto computeAngleTFromDir = [&](double dir_mils) -> double
{
    if(guns.empty())
        return 0.0;

    if(base_piece_index < 0 || base_piece_index >= (int)guns.size())
        return 0.0;

    // Vector PIEZA BASE -> TARGET
    double dx = tgt_e - guns[base_piece_index].e;
    double dy = tgt_n - guns[base_piece_index].n;

    double az = std::atan2(dx, dy);

    if(az < 0)
        az += 2.0 * PI;

    double gun_target_mils = az * (6400.0 / (2.0 * PI));

    // Redondeo igual que en renderFire
    gun_target_mils = std::floor((gun_target_mils + 1.5) / 2.0) * 2.0;

    double angle_t = std::fabs(dir_mils - gun_target_mils);

    if(angle_t > 3200.0)
        angle_t = 6400.0 - angle_t;

    return std::round(angle_t);
};

//////////////////////////////////////////////////
// AZ ACTUAL PIEZA BASE -> TARGET
//////////////////////////////////////////////////

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

    // mismo redondeo que renderFire
    az_mils = std::floor((az_mils + 1.5) / 2.0) * 2.0;

    while(az_mils < 0)
        az_mils += 6400.0;

    while(az_mils >= 6400.0)
        az_mils -= 6400.0;

    return std::round(az_mils);
};

//////////////////////////////////////////////////
// CONVERTIR CORRECCION DEL OBSERVADOR A PIEZA
//////////////////////////////////////////////////

auto applyObserverCorrection = [&](double obs_lr, double obs_ad, double obs_ud, double dir_mils)
{
    // Las correcciones L/R y A/D que entran aqui son del OBSERVADOR.
    //
    // obs_lr:
    //   R positivo
    //   L negativo
    //
    // obs_ad:
    //   A positivo
    //   D negativo
    //
    // dir_mils:
    //   DIR del observador.
    //
    // ANG T se calcula entre DIR y AZ pieza-target.
    // Luego se convierten L/R y A/D al sistema real de la pieza.

    while(dir_mils < 0.0)
        dir_mils += 6400.0;

    while(dir_mils >= 6400.0)
        dir_mils -= 6400.0;

    double angle_t = computeAngleTFromDir(dir_mils);
    double t_rad = angle_t * (2.0 * PI / 6400.0);

    double real_lr = (obs_lr * std::cos(t_rad)) - (obs_ad * std::sin(t_rad));
    double real_ad = (obs_ad * std::cos(t_rad)) - (obs_lr * std::sin(t_rad));

    // La correccion ya quedo convertida al sistema pieza-target.
    // Por eso renderFire debe aplicar el vector usando el AZ real
    // pieza -> target, no el DIR del observador.
    shift_angle = computeBaseAz();

    shift_lr += real_lr;
    shift_ad += real_ad;
    shift_ud += obs_ud;

    ud_active = false;
    ud_corr = 0.0;
};

//////////////////////////////////////////////////
// PREPARAR TGT BASE PARA CORRECCIONES OBSERVADOR
//////////////////////////////////////////////////

auto prepareTgtBaseShotForObserverCorrections = [&]()
{
    // ============================================================
    // TGT 1 / TGT 2:
    // Estos menús crean un blanco nuevo o derivado y hacen un
    // disparo base.
    //
    // Luego, si el operador entra por:
    //
    //   COMP CORR -> N
    //
    // la nueva DIR y las correcciones L/R A/D U/D deben ser
    // interpretadas desde el punto de vista del OBSERVADOR.
    //
    // IMPORTANTE:
    // NO se deben borrar las correcciones de REG/FM2 aquí.
    //
    // Estas variables deben conservarse:
    //
    //   reg_dist
    //   reg_def
    //   df_corr
    //   time_reg_correction
    //
    // Porque son la registración/corrección activa que ya acercó
    // el sistema a la hoja. Si se limpian, TGT 1 y TGT 2 vuelven
    // a salir crudos.
    //
    // Aquí solamente limpiamos correcciones instantáneas del impacto
    // anterior y estados temporales del observador.
    // ============================================================

    fire_phase = 0;

    // ============================================================
    // NO TOCAR:
    //
    // reg_dist
    // reg_def
    // df_corr
    // time_reg_correction
    //
    // Se conservan a propósito.
    // ============================================================

    // Shift instantáneo acumulado del impacto/blanco anterior.
    // Esto sí debe limpiarse al crear un nuevo TGT base, para que
    // el nuevo blanco no herede una corrección L/R A/D U/D anterior.
    shift_lr = 0.0;
    shift_ad = 0.0;
    shift_ud = 0.0;

    // Corrección vertical temporal.
    ud_active = false;
    ud_corr = 0.0;

    // Estado instantáneo del observador.
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

    // Estado doctrinal de salida.
    all_guns_command = false;
    ffe_mode = false;

    // Permitir edición de carga después del disparo base, igual que
    // el flujo normal de fuego.
    chg_allowed = true;
};

auto finishTgtBaseShot = [&]() -> std::string
{
    prepareTgtBaseShotForObserverCorrections();

    current_menu = "FM1_BASE_PIECE";

    return "BASE PIECE (P *): " + std::to_string(base_piece_index + 1);
};

//////////////////////////////////////////////////
// EOM
//////////////////////////////////////////////////

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

//////////////////////////////////////////////////
// 🔥 GUARDAR REPORTE EN ARCHIVO
//////////////////////////////////////////////////

    std::vector<std::string> pdf_lines;

    pdf_lines.push_back("====================");
    pdf_lines.push_back("    ARTILLERIA");
    pdf_lines.push_back("    AD GLORIAM");
    pdf_lines.push_back("====================");
    pdf_lines.push_back("");
    pdf_lines.push_back("REPORTE DE MISION");

    if(!mission_log.empty())
    {
        const auto& shot = mission_log.back(); // SOLO EL ÚLTIMO

        std::stringstream ss(shot.fire);
        std::string line;

        bool capture = false;

        while(std::getline(ss, line))
        {
            if(line.find("----- PIECE") != std::string::npos)
            {
                int piece_num = -1;

                std::stringstream ss(line);
                std::string tmp;

                ss >> tmp >> tmp >> piece_num;

                if(piece_num == base_piece_index + 1)
                {
                    capture = true;
                    pdf_lines.push_back(line);
                }
                else
                {
                    capture = false;
                }

                continue;
            }

            std::string clean = normStr(line);

            if(capture && !clean.empty())
                pdf_lines.push_back(clean);
        }
    }

    if(!mission_log.empty())
    {
        pdf_lines.push_back("FECHA: " + mission_log.front().timestamp);
        pdf_lines.push_back("");
    }

    pdf_lines.push_back("TARGET DATA");

    std::stringstream ss_alt, ss_n, ss_e;
    ss_alt << (int)tgt_alt;
    ss_n << (int)tgt_n;
    ss_e << (int)tgt_e;

    pdf_lines.push_back("ALT " + ss_alt.str());
    pdf_lines.push_back("N " + ss_n.str());
    pdf_lines.push_back("E " + ss_e.str());
    pdf_lines.push_back("");

// ==============================
// 🔥 AGRUPACIÓN DOCTRINAL POR FM
// ==============================

std::vector<ShotLog> fm1;
std::vector<ShotLog> fm2_reg;
std::vector<ShotLog> fm2_time;
std::vector<ShotLog> fm3;
std::vector<ShotLog> fm4;

for(const auto& shot : mission_log)
{
    if(shot.label == "BASE FIRE")
        fm1.push_back(shot);

    else if(shot.label == "REGISTRATION")
        fm2_reg.push_back(shot);

    else if(shot.label == "TIME REG")
        fm2_time.push_back(shot);

    else if(shot.label == "SHIFT (FM3)")
        fm3.push_back(shot);

    else if(shot.label == "PMI (FM4)")
        fm4.push_back(shot);
}

// ==============================
// 🔥 FUNCIÓN PARA IMPRIMIR BLOQUES
// ==============================

auto printFMBlock = [&](const std::string& title, const std::vector<ShotLog>& logs)

{
    if(logs.empty()) return;

    pdf_lines.push_back("------------------------------------------------------------");
    pdf_lines.push_back(title);
    pdf_lines.push_back("------------------------------------------------------------");
    pdf_lines.push_back("");

    for(const auto& shot : logs)
    {
        // INPUTS
        if(!shot.inputs.empty())
        {
            pdf_lines.push_back("INPUTS:");

            std::stringstream ss_in(shot.inputs);
            std::string line;

            while(std::getline(ss_in, line))
            {
                if(!line.empty())
                    pdf_lines.push_back(line);
            }

            pdf_lines.push_back("");
        }

        // FIRE RESULT
        std::stringstream ss_fire(shot.fire);
        std::string line;

        while(std::getline(ss_fire, line))
            pdf_lines.push_back(line);

        pdf_lines.push_back("");
    }
};

// ==============================
// 🔥 IMPRIMIR POR FASE
// ==============================

if(!fm1.empty())
{

    // printFMBlock("FM1 - BASE FIRE", fm1);
}

if(!fm2_reg.empty())
{

    // printFMBlock("FM2 - REGISTRATION", fm2_reg);
}

if(!fm2_time.empty())
{

   // printFMBlock("FM2 - TIME REG", fm2_time);
}

if(!fm3.empty())
{

   // printFMBlock("FM3 - SHIFT", fm3);
}

if(!fm4.empty())
{

   // printFMBlock("FM4 - PMI", fm4);
}
    pdf_lines.push_back("");
    pdf_lines.push_back("MISSION CLOSED");

    saveReportPDF(pdf_lines, mission_counter);

    mission_counter++;

    mission_active=false;
    ffe_mode=false;

    return out.str() + "MAIN (? 1 3 4 5 7 X *)";
}

//////////////////////////////////////////////////
// RESET A MAIN CON *
//////////////////////////////////////////////////

    if(cmd=="*")
    {
        current_menu="MAIN";
        return drPrefix("MAIN",mainMenu());
    }

//////////////////////////////////////////////////
// MAIN
//////////////////////////////////////////////////

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
            current_menu="OBS";
            input_stage=0;
            return "OBS ALT:";
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

//////////////////////////////////////////////////
// TARGET
//////////////////////////////////////////////////

if(current_menu=="TARGET")
{
    static int temp_knpt = 0;

    // 🔙 BACK CON P
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

    // ==============================
    // PASO 0: INDICADOR TGT
    // Ejemplo: AA100
    // ==============================
    if(input_stage==0)
    {
        if(!v.empty())
            current_tgt_indicator = v;

        input_stage++;
        return "TGT/KNPT # (P *): " + std::to_string(temp_knpt);
    }

    // ==============================
    // PASO 1: KNPT #
    // ==============================
    if(input_stage==1)
    {
        if(!v.empty())
            temp_knpt = std::stoi(v);

        input_stage++;
        return "TGT E (P *): " + std::to_string((int)tgt_e);
    }

    // ==============================
    // PASO 2: EASTING
    // ==============================
    if(input_stage==2)
    {
        if(!v.empty())
            tgt_e = std::stod(v);

        input_stage++;
        return "TGT N (P *): " + std::to_string((int)tgt_n);
    }

    // ==============================
    // PASO 3: NORTHING
    // ==============================
    if(input_stage==3)
    {
        if(!v.empty())
            tgt_n = std::stod(v);

        input_stage++;
        return "TGT ALT (P *): " + std::to_string((int)tgt_alt);
    }

    // ==============================
    // PASO 4: ALTURA
    // ==============================
    if(input_stage==4)
    {
        if(!v.empty())
            tgt_alt = std::stod(v);

        // 🔥 GUARDAR EN MAPA
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

//////////////////////////////////////////////////
// AFU
//////////////////////////////////////////////////

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

//////////////////////////////////////////////////
// ARTILLERY TYPE
//////////////////////////////////////////////////

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

//////////////////////////////////////////////////
// AMMO
//////////////////////////////////////////////////

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

//////////////////////////////////////////////////
// COB DOCTRINAL (GB + DIR DIST IV)
//////////////////////////////////////////////////

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

        // 🔥 AQUÍ GUARDAS TODO PARA EL PDF
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

        if(temp_dist <= 0)
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

        // 🔥 RESET TEMPORALES PARA NUEVA PIEZA
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

//////////////////////////////////////////////////
// OBS
//////////////////////////////////////////////////

if(current_menu=="OBS")
{
    // 🔙 BACK CON P
    if(cmd=="P")
    {
        if(input_stage > 0) input_stage--;

        switch(input_stage)
        {
            case 0: return "OBS ALT (P *): " + std::to_string((int)obs_alt);
            case 1: return "OBS E (P *): " + std::to_string((int)obs_e);
            case 2: return "OBS N (P *): " + std::to_string((int)obs_n);
        }
    }

    std::string v = normStr(cmd);

    if(input_stage==0)
    {
        if(!v.empty()) obs_alt = std::stod(v);
        input_stage++;
        return "OBS E (P *): " + std::to_string((int)obs_e);
    }

    if(input_stage==1)
    {
        if(!v.empty()) obs_e = std::stod(v);
        input_stage++;
        return "OBS N (P *): " + std::to_string((int)obs_n);
    }

    if(input_stage==2)
    {
        if(!v.empty()) obs_n = std::stod(v);

        main_inputs.push_back("OBSERVER");
        main_inputs.push_back("ALT " + std::to_string((int)obs_alt));
        main_inputs.push_back("E " + std::to_string((int)obs_e));
        main_inputs.push_back("N " + std::to_string((int)obs_n));

        current_menu = "MAIN";
        input_stage = 0;

        return "OBS STORED\nMAIN (? 1 3 4 5 7 X *)";
    }
}

//////////////////////////////////////////////////
// MAP MODEL
//////////////////////////////////////////////////

if(current_menu=="MAP_MODEL")
{
    // 🔙 BACK CON P
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

    // 🔥 SI HAY VALOR → GUARDAR
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

    // 🔥 SIEMPRE AVANZAR (ENTER o valor)
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

            current_menu="MAIN";
            input_stage=0;
            return "MAP STORED\nMAIN (? 1 3 4 5 7 X *)";
        }
    }

    return "MAX E (P *): " + std::to_string((int)map_e_max);
}
//////////////////////////////////////////////////
// MET
//////////////////////////////////////////////////

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
//////////////////////////////////////////////////
// FM
//////////////////////////////////////////////////

    if(current_menu=="FM")
    {
        // 🔥 VOLVER A CARGA AUTOMATICA
        
        if(cmd=="AUTOCHG")
        {
            manual_chg_enabled = false;
            manual_chg_value = 0;

            return "AUTO CHARGE ENABLED\nFM (? 1 2 3 4 S P X *)";
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

        if(cmd=="3")    // 🔥 FM3 DOCTRINAL
        {
            current_menu = "SHIFT_PREV_DIR";
            return "PREV DIR:";
        }

        if(cmd=="4")   // 🔥 FM4 E.A. Y P.M.I.
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

            // COB
            for(size_t i=0;i<guns.size();i++)
            {
                out<<"COB "<<i+1<<" ALT "<<guns[i].alt
                <<" N "<<guns[i].n
                <<" E "<<guns[i].e<<"\n";
            }
            out << "BASE PIECE " << (base_piece_index + 1) << "\n";
            out << "AZ LAY " << az_lay << "\n";
            out << "REF DEF " << def_base << "\n";
            // TARGET
            out<<"TGT ALT "<<tgt_alt
            <<" N "<<tgt_n
            <<" E "<<tgt_e<<"\n";

            // AMMO
            out<<"AMMO "<<ammo_proj_prop
            <<" LOT "<<ammo_proj_lot
            <<" WT "<<ammo_proj_wt<<"\n";

            // OBS
            out<<"OBS ALT "<<obs_alt
            <<" E "<<obs_e
            <<" N "<<obs_n
            <<" GZ "<<obs_gz<<"\n";

            // MAP
            out<<"MAP EMAX "<<map_e_max
            <<" EMIN "<<map_e_min
            <<" NMAX "<<map_n_max
            <<" NMIN "<<map_n_min<<"\n";

            return out.str() + "FM (? 1 2 3 4 R E P X *)";
        }

        // 🔥 DOCTRINAL: A = TODAS LAS PIEZAS (FFE)
        if(cmd=="A")
        {
            all_guns_command = true;
            ffe_mode = true;

            return "FFE\nPRESS X";
        }

        // SHIFT oculto
       if(cmd=="T")
        {
            current_menu="SHIFT_PREV_DIR";
            return "PREV DIR:";
        }

       if(cmd=="X")
        {
            // 🔥 VALIDACIÓN DOCTRINAL PRO

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

            all_guns_command = false; // reset doctrinal

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


            // 🔥 2. INPUTS DEL DISPARO
            for(const auto& s : last_inputs)
                input_ss << s << "\n";

            shot.inputs = input_ss.str();

            // 🔥 LIMPIAR SOLO inputs del disparo
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

//////////////////////////////////////////////////
// FM1 - TGT SUBMENU
//////////////////////////////////////////////////

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

//////////////////////////////////////////////////
// FM1 - CUADRICULA
//////////////////////////////////////////////////

if(current_menu=="FM1_GRID")
{
    // 🔙 BACK CON P
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

//////////////////////////////////////////////////
// FM1 - TRANSPORTE
//////////////////////////////////////////////////

if(current_menu=="FM1_TRANSPORT")
{
    // 🔙 BACK CON P
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

            // TGT2 es un blanco transportado temporal.
            // NO sobrescribe el KNPT de origen.
            // Así FM2 puede seguir usando el TGT original.
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

//////////////////////////////////////////////////
// FM1 - POLARES
//////////////////////////////////////////////////

if(current_menu=="FM1_POLAR")
{
    // 🔙 BACK
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

//////////////////////////////////////////////////
// FM4 - E.A. Y P.M.I.
//////////////////////////////////////////////////

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

        // Preguntar si hay más impactos
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
            // 🔥 CALCULO PMI
            double avg_lr=0, avg_ad=0, avg_ud=0;

            for(double v:fm4_lr) avg_lr+=v;
            for(double v:fm4_ad) avg_ad+=v;
            for(double v:fm4_ud) avg_ud+=v;

            if(!fm4_lr.empty()) avg_lr/=fm4_lr.size();
            if(!fm4_ad.empty()) avg_ad/=fm4_ad.size();
            if(!fm4_ud.empty()) avg_ud/=fm4_ud.size();

            // Aplicar PMI como correccion observada.
            // Usa el ultimo DIR confirmado en COMP CORR / SHIFT.
            applyObserverCorrection(
                avg_lr,
                avg_ad,
                avg_ud,
                inst_last_dir
            );

            // limpiar buffers
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
//////////////////////////////////////////////////
// SHIFT DOCTRINAL
//////////////////////////////////////////////////

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

    // ENTER confirma ANG T calculado.
    // Si el usuario escribe algo, lo aceptamos como ANG T manual.
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

//////////////////////////////////////////////////
// SHIFT MENU
//////////////////////////////////////////////////

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

//////////////////////////////////////////////////
// SHEAF MENU
//////////////////////////////////////////////////

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
            current_menu="SHEAF_WIDTH"; // 🔥 NUEVO PASO
            return "OPEN WIDTH (MILS):";
        }

        return "SHEAF (CONV/OPEN):";
    }

//////////////////////////////////////////////////
// SHEAF WIDTH (NUEVO)
//////////////////////////////////////////////////

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

//////////////////////////////////////////////////
// REG
//////////////////////////////////////////////////

   if(current_menu=="REG")
{
        // 🔥 PREGUNTA REUTILIZAR REG
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

            // 🔥 CARGAR TARGET DESDE MAIN 3
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

            // RECALCULAR LA SOLUCION BASE YA CON EL NUEVO REG_DIST
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

            // 🔥 GUARDAR VALOR ACTUAL
            double reg_def_backup = reg_def;

            // 🔥 FORZAR SIN REG
            reg_def = 0;

            // ===============================
            // 🔥 USAR MISMO TARGET AJUSTADO QUE renderFire
            // ===============================
            double adj_tgt_e = tgt_e;
            double adj_tgt_n = tgt_n;

            if(shift_lr != 0 || shift_ad != 0)
            {
                double dir_rad = shift_angle * (2.0 * PI / 6400.0);

                // Vector A/D real: sobre la linea pieza-target.
                double ad_e = std::sin(dir_rad);
                double ad_n = std::cos(dir_rad);

                // Vector L/R real: perpendicular a la linea pieza-target.
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
            int chg=0;

            solve(ammo_proj_prop, dist, chg, qe, tof, drift);

            if(std::abs(drift) < 0.001)
            {
                drift = 0.00038 * dist;
            }

            // 🔥 DEF REAL LIMPIA
            double def_real = def_base + (mils - az_lay);
            def_real += df_corr;

            double drift_accum_real = drift * (tof / 25.0);
            double jump_h_real = 0.3;

            def_real += drift_accum_real;
            def_real += jump_h_real;

            def_real = std::round(def_real);

            while(def_real < 0) def_real += 6400;
            while(def_real >= 6400) def_real -= 6400;

            // 🔥 RESTAURAR
            reg_def = reg_def_backup;

            // ================================
            // 🔥 CORRECCIÓN REG REAL (CONV)
            // ================================

            // detectar si operador no corrigió
            if(std::abs(reg_input - last_def_solution) <= 1)
            {
                // 🔥 calcular spread entre piezas
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
                int chg_tmp=0;

                solve(ammo_proj_prop, dist, chg_tmp, qe_tmp, tof_tmp, drift_tmp);

                if(std::abs(drift_tmp) < 0.001)
                {
                    drift_tmp = 0.00038 * dist;
                }

                double def_temp = def_base + (mils - az_lay);
                def_temp += reg_def + df_corr;

                double drift_accum = drift_tmp * (tof_tmp / 25.0);
                double jump_h = 0.3;

                def_temp += drift_accum;
                def_temp += jump_h;

                def_temp = std::round(def_temp);

                while(def_temp < 0) def_temp += 6400;
                while(def_temp >= 6400) def_temp -= 6400;

                if(def_temp > max_def) max_def = def_temp;
                if(def_temp < min_def) min_def = def_temp;
            }

                double error = (max_def - last_def_solution);

                // 🔥 GANANCIA DOCTRINAL (AJUSTE FINAL)
                reg_def += error * 2.5;
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

//////////////////////////////////////////////////
// REG - CONFIRMAR BASE PIECE DESPUES DE FUZE
//////////////////////////////////////////////////

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

    // Recalcular solucion base usando la nueva BASE PIECE.
    // Esto actualiza last_dist_solution, last_def_solution y last_qe_solution.
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

//////////////////////////////////////////////////
// FM1 - CONFIRMAR BASE PIECE ANTES DEL DISPARO
//////////////////////////////////////////////////

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

    // IMPORTANTE:
    // TGT1 usa DEF reverse solo para el disparo base.
    // Despues del resultado, debe apagarse para que COMP CORR -> N,
    // INST_UD_SHIFT, FM3 y demas correcciones no hereden esa logica.
    fm1_base_def_reverse = false;
    active_fm1_def_reverse = false;
    active_fm1_transport_qe_shape = false;

    return result + "COMP CORR (Y N P *)\nTESON EVIL";
}

//////////////////////////////////////////////////
// COMP CORR
//////////////////////////////////////////////////

if(current_menu=="COMP_CORR")
{
    // SOLO permitir cambio de carga si viene de un disparo
    if((cmd=="" || cmd=="X") && chg_allowed)
    {
        chg_allowed = false;
        chg_edit_mode = true;
        chg_wait_value = false;
        current_menu = "CHG_EDIT";
        return "LOT: ?";
    }

    // Y = TIME REG
    if(cmd=="Y")
    {
        current_menu = "TIME_REG";
        return "TIME REG (Y N P *)";
    }

    // N = ESP. INSTANTANEA / CORRECCIONES PDA
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

    // Limpiar datos previos de la corrección instantánea
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

    // P = VOLVER AL MENU FM
    if(cmd=="P")
    {
        current_menu = "FM";
        return drPrefix("FM", fireMenu());
    }

    return "COMP CORR (Y N P *)";
}

//////////////////////////////////////////////////
// ESP. INSTANTANEA / PDA SHIFT CORR
//////////////////////////////////////////////////

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

    // Si el usuario escribe un DIR nuevo, se guarda.
    // Si solo da ENTER, se confirma el DIR mostrado.
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

    // DIR solo se usa para calcular ANG T.
    // No se usa directamente como direccion del vector final,
    // porque la correccion se convierte luego en INST_UD_SHIFT.

    // ANG T se calcula usando el DIR confirmado
    // contra la línea PIEZA BASE -> TARGET
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

    // ENTER confirma el ANG T calculado.
    // No se escribe manualmente.
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

    // ============================================================
    // APLICAR CORRECCION INSTANTANEA COMPLETA
    // ============================================================
    //
    // En COMP CORR -> N, las correcciones L/R y A/D son del OBSERVADOR.
    // Por eso NO se deben sumar directo a shift_lr / shift_ad.
    //
    // Primero se convierten usando ANG T:
    //
    //   inst_lr_shift = R positivo, L negativo
    //   inst_ad_shift = A positivo, D negativo
    //
    // Con ANG T cerca de 1600 mils:
    //   R/L del observador se convierte casi en alcance.
    //   A/D del observador se convierte casi en lateral.
    //
    // Ejemplo:
    //   AZ 3318
    //   DIR 4930
    //   ANG T 1612
    //   R200
    //
    // Debe afectar principalmente DIST/QE/TOF, no DEF.
    // ============================================================

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

//////////////////////////////////////////////////
// TIME REG / FUZE DOCTRINAL
//////////////////////////////////////////////////

    if(current_menu=="TIME_REG")
    {
        // Flujo doctrinal del manual
        if(cmd=="Y")
        {
            current_menu="TIME_REG_FUZE";
            fire_phase = 2;
            return std::string("FUZE (P*) ") + (fuze_time_mode ? "TIA" : "PDA");
        }

        // Mantiene compatibilidad con la prueba vieja
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
            // PDA normal para este TIME REG.
            // No queda ningun modo temporal activo.
            fuze_time_mode = false;
            hob = 0;
            time_reg_fuze_temporary = false;

            current_menu = "UD_CORR";
            return "(U/D) CORR (*)";
        }

        if(cmd=="TIA")
        {
            // TIA temporal solo para TIME REG y su U/D CORR.
            // Despues de aplicar U/D CORR se restablece a PDA.
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

        // Mantener TIA activo solamente para el resultado final
        // despues de aplicar U/D CORR.
        fuze_time_mode = true;
        time_reg_fuze_temporary = true;

        current_menu = "UD_CORR";
        return "(U/D) CORR (*)";
    }

//////////////////////////////////////////////////
// U/D CORR
//////////////////////////////////////////////////

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

//////////////////////////////////////////////////
// DF CORR
//////////////////////////////////////////////////

if(current_menu=="DF_CORR")
{
    std::stringstream ss(cmd);
    std::string dir; 
    double val;

    ss >> dir >> val;

    // 🔥 DISTANCIA BASE
    double base_dist = last_dist_solution;

    if(base_dist <= 0)
        return "NO BASE DIST";

    // 🔥 convertir metros → mils
    double mils = (val / base_dist) * 1000.0;

    // ===============================
    // 🔥 ADD / DROP (CORREGIDO REAL)
    // ===============================
    if(dir=="ADD" || dir=="DROP")
    {
        double new_dist;

    // 🔥 ACUMULACIÓN DOCTRINAL REAL

    if(dir=="ADD")
    {
        reg_dist += val;
    }
    else // DROP
    {
        reg_dist -= val;
    }

    // 🔥 reset QE correction
    time_reg_correction = 0;
    }

    // ===============================
    // 🔥 RIGHT / LEFT
    // ===============================
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

//////////////////////////////////////////////////
// CHG EDIT (DOCTRINAL)
//////////////////////////////////////////////////

if(current_menu=="CHG_EDIT")
{
    // Paso 1: LOT
    if(!chg_wait_value)
    {
        chg_wait_value = true;
        return "Cg:";
    }

    // Paso 2: nueva carga
    try
    {
        int val = std::stoi(cmd);

        manual_chg_enabled = true;
        manual_chg_value = val;

        last_inputs.push_back("CHG " + std::to_string(val));

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

//////////////////////////////////////////////////
// DEFAULT
//////////////////////////////////////////////////

    return drPrefix("MAIN",mainMenu());

    } 
//////////////////////////////////////////////////
// MENUS
//////////////////////////////////////////////////

std::string BasicEngine::mainMenu(){ return "MAIN (? 1 3 4 5 7 X *)"; }
std::string BasicEngine::fireMenu(){ return "FM (? 1 2 3 4 R E P X *)"; }
std::string BasicEngine::afuMenu(){ return "AFU INDEX (? 1 3 5 *)"; }
std::string BasicEngine::ammoMenu(){ return "AMMO FILE (? I *)"; }

//////////////////////////////////////////////////
// RESET
//////////////////////////////////////////////////

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

    obs_alt=0;
    obs_n=0;
    obs_e=0;

    wind_dir=0;
    wind_speed=0;
    temperature=0;

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
    manual_chg_value = 0;
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

    // intento 1
    std::ifstream f1("tables.csv");
    if(f1.good())
    {
        loadTablesFromCSV("tables.csv");
        loaded = true;
    }

    // intento 2
    if(!loaded)
    {
        std::ifstream f2("backend/tables.csv");
        if(f2.good())
        {
            loadTablesFromCSV("backend/tables.csv");
            loaded = true;
        }
    }

    // intento 3 (ruta absoluta desde exe)
    // Windows usa GetModuleFileNameA.
    // Linux/WSL usa /proc/self/exe.
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