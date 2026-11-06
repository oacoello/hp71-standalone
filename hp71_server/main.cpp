#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <conio.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <filesystem>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <algorithm>

#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#endif

#include "httplib.h"
#include "basic_engine.h"

BasicEngine engine;

//////////////////////////////////////////////////
// EXE DIRECTORY CROSS-PLATFORM
//////////////////////////////////////////////////

std::filesystem::path getExeDir()
{
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    return std::filesystem::path(exePath).parent_path();
#else
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);

    if(len > 0)
    {
        exePath[len] = '\0';
        return std::filesystem::path(exePath).parent_path();
    }

    return std::filesystem::current_path();
#endif
}

//////////////////////////////////////////////////
// RECORD DE TIRO + HISTORIAL DE INPUTS
//////////////////////////////////////////////////

std::mutex g_stateMutex;
std::string g_recordJson = "{\"data\":{},\"rows\":[]}"; // estado del record (lo manda el frontend)

// Escapa strings para construir JSON a mano (sin libreria externa)
std::string jsonEscape(const std::string& in)
{
    std::string out;
    out.reserve(in.size() + 8);

    for(unsigned char c : in)
    {
        switch(c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if(c < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else out += (char)c;
        }
    }

    return out;
}

// Solo letras/digitos/._-  (evita path traversal en sessionId)
std::string safeSegment(const std::string& value, const std::string& fallback)
{
    std::string clean;

    for(char c : value)
    {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        clean += ok ? c : '-';
    }

    while(!clean.empty() && clean.front() == '-') clean.erase(clean.begin());
    while(!clean.empty() && clean.back() == '-')  clean.pop_back();

    return clean.empty() ? fallback : clean;
}

std::string nowTimestamp()
{
    std::time_t tt = std::time(nullptr);
    std::tm tmv{};

#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

std::string fileTimeToIso(const std::filesystem::file_time_type& ftime)
{
    // Convierte file_time_type -> system_clock (portable sin C++20)
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());

    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm tmv{};

#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmv);
    return buf;
}

// Carpeta de logs: junto al exe; si no se puede escribir, usa TEMP
std::filesystem::path getLogDir()
{
    std::filesystem::path dir = getExeDir() / "hp71-logs";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    if(ec)
    {
        dir = std::filesystem::temp_directory_path() / "hp71-logs";
        std::filesystem::create_directories(dir, ec);
    }

    return dir;
}

// Ruta del record de tiro persistido en disco (sobrevive reinicios)
std::filesystem::path getRecordPath()
{
    return getLogDir() / "record.json";
}

// Carga un asset estatico desde las mismas rutas que index.html
bool loadAsset(const std::string& name, std::string& out)
{
    std::filesystem::path exeDir = getExeDir();

    std::vector<std::filesystem::path> paths = {
        exeDir / name,
        exeDir / ("web_ui/" + name),
        exeDir / ("../web_ui/" + name),
        exeDir / ("../../web_ui/" + name),
        exeDir / ("../../../web_ui/" + name),   // build/hp71_server/Release -> raiz del repo

        exeDir / ("backend/web_ui/" + name),
        exeDir / ("../backend/web_ui/" + name),
        exeDir / ("../../backend/web_ui/" + name)
    };

    for(const auto& p : paths)
    {
        std::ifstream file(p, std::ios::binary);

        if(file.is_open())
        {
            out.assign(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>()
            );
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////
// SERVER THREAD
//////////////////////////////////////////////////

void runServer()
{
    httplib::Server server;

    // Restaurar el record de tiro guardado en disco (si existe)
    {
        std::ifstream recordFile(getRecordPath(), std::ios::binary);
        if(recordFile)
        {
            std::string saved((std::istreambuf_iterator<char>(recordFile)), std::istreambuf_iterator<char>());
            if(!saved.empty())
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                g_recordJson = saved;
            }
        }
    }

    server.Post("/input", [](const httplib::Request& req, httplib::Response& res)
    {
        std::string cmd = req.get_param_value("cmd");
        std::string result = engine.execute(cmd);
        res.set_content(result, "text/plain");
    });

    // ----- RECORD DE TIRO: estado JSON (lo mantiene el frontend) -----
    server.Get("/record", [](const httplib::Request&, httplib::Response& res)
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        res.set_content(g_recordJson, "application/json");
    });

    server.Post("/record", [](const httplib::Request& req, httplib::Response& res)
    {
        std::string json;
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_recordJson = req.body.empty() ? "{\"data\":{},\"rows\":[]}" : req.body;
            json = g_recordJson;
        }

        // Persistir en disco
        std::ofstream file(getRecordPath(), std::ios::binary | std::ios::trunc);
        if(file) file << json;

        res.set_content("{\"success\":true}", "application/json");
    });

    // ----- HISTORIAL DE INPUTS: archivos .log por sesion -----
    server.Get("/log", [](const httplib::Request& req, httplib::Response& res)
    {
        std::filesystem::path dir = getLogDir();
        std::string sessionId = req.get_param_value("session");

        // Contenido de una sesion
        if(!sessionId.empty())
        {
            std::string id = safeSegment(sessionId, "session");
            std::ifstream file(dir / (id + ".log"), std::ios::binary);
            std::string content;

            if(file.is_open())
            {
                content.assign(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );
            }

            res.set_content(content, "text/plain; charset=utf-8");
            return;
        }

        // Lista de sesiones
        std::vector<std::filesystem::path> files;
        std::error_code ec;

        for(const auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            if(entry.is_regular_file() && entry.path().extension() == ".log")
                files.push_back(entry.path());
        }

        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b)
        {
            return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
        });

        std::string json = "{\"logs\":[";
        bool first = true;

        for(const auto& p : files)
        {
            std::ifstream file(p, std::ios::binary);
            std::string content;

            if(file.is_open())
            {
                content.assign(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );
            }

            // lineas no vacias + preview de las ultimas 3
            std::vector<std::string> lines;
            std::istringstream ss(content);
            std::string line;

            while(std::getline(ss, line))
            {
                if(!line.empty() && line.back() == '\r') line.pop_back();
                if(!line.empty()) lines.push_back(line);
            }

            std::string previews;
            size_t start = lines.size() > 3 ? lines.size() - 3 : 0;

            for(size_t i = start; i < lines.size(); i++)
            {
                if(i > start) previews += ",";
                previews += "\"" + jsonEscape(lines[i]) + "\"";
            }

            std::error_code ec2;
            auto ftime = std::filesystem::last_write_time(p, ec2);
            std::string ts = ec2 ? "" : fileTimeToIso(ftime);
            std::string id = p.stem().string();

            if(!first) json += ",";
            first = false;

            json += "{\"id\":\"" + jsonEscape(id) +
                    "\",\"fileName\":\"" + jsonEscape(id + ".log") +
                    "\",\"createdAt\":\"" + jsonEscape(ts) +
                    "\",\"updatedAt\":\"" + jsonEscape(ts) +
                    "\",\"size\":" + std::to_string(content.size()) +
                    ",\"entries\":" + std::to_string(lines.size()) +
                    ",\"preview\":[" + previews + "]}";
        }

        json += "]}";
        res.set_content(json, "application/json");
    });

    server.Post("/log", [](const httplib::Request& req, httplib::Response& res)
    {
        std::string type = req.get_param_value("type");
        std::string content = req.get_param_value("content");
        std::string sessionId = req.get_param_value("sessionId");

        if(type.empty() || content.empty())
        {
            res.status = 400;
            res.set_content("{\"error\":\"Faltan campos\"}", "application/json");
            return;
        }

        std::string id = safeSegment(sessionId, "session");
        std::filesystem::path dir = getLogDir();

        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        std::ofstream file(dir / (id + ".log"), std::ios::app);

        if(!file.is_open())
        {
            res.status = 500;
            res.set_content("{\"error\":\"No se pudo escribir el log\"}", "application/json");
            return;
        }

        file << "[" << nowTimestamp() << "] " << type << ": " << content << "\n";
        res.set_content("{\"success\":true}", "application/json");
    });

    server.Delete("/log", [](const httplib::Request& req, httplib::Response& res)
    {
        std::string sessionId = req.get_param_value("session");

        if(sessionId.empty())
        {
            res.status = 400;
            res.set_content("{\"error\":\"Falta sessionId\"}", "application/json");
            return;
        }

        std::string id = safeSegment(sessionId, "session");
        std::error_code ec;
        std::filesystem::remove(getLogDir() / (id + ".log"), ec);

        res.set_content("{\"success\":true}", "application/json");
    });

    // ----- Iconos y assets visuales para barra de tareas y titulo de ventana -----
    server.Get("/icon.ico", [](const httplib::Request&, httplib::Response& res)
    {
        std::string content;
        if(loadAsset("icon.ico", content))
        {
            res.set_header("Cache-Control", "public, max-age=86400");
            res.set_content(content, "image/x-icon");
        }
        else
        {
            res.status = 404;
        }
    });

    server.Get("/favicon.ico", [](const httplib::Request&, httplib::Response& res)
    {
        std::string content;
        if(loadAsset("icon.ico", content))
        {
            res.set_header("Cache-Control", "public, max-age=86400");
            res.set_content(content, "image/x-icon");
        }
        else
        {
            res.status = 404;
        }
    });

    server.Get("/icon.png", [](const httplib::Request&, httplib::Response& res)
    {
        std::string content;
        if(loadAsset("icon.png", content) || loadAsset("Splash_screen_hp71b.png", content))
        {
            res.set_header("Cache-Control", "public, max-age=86400");
            res.set_content(content, "image/png");
        }
        else
        {
            res.status = 404;
        }
    });

    server.Get("/manifest.json", [](const httplib::Request&, httplib::Response& res)
    {
        std::string content;
        if(loadAsset("manifest.json", content))
        {
            res.set_header("Cache-Control", "public, max-age=86400");
            res.set_content(content, "application/json");
        }
        else
        {
            res.set_content("{\"name\":\"SANTA BARBARA\",\"short_name\":\"HP71B\",\"display\":\"standalone\",\"icons\":[{\"src\":\"/icon.ico\",\"sizes\":\"256x256\",\"type\":\"image/x-icon\"}]}", "application/json");
        }
    });

    // ----- Hoja del record de tiro (PDF convertido, autocontenido) -----
    server.Get("/record-de-tiro.html", [](const httplib::Request&, httplib::Response& res)
    {
        std::string content;

        if(loadAsset("record-de-tiro.html", content))
        {
            // Sin cache: la hoja/JS cambian con las actualizaciones de la UI
            res.set_header("Cache-Control", "no-store, must-revalidate");
            res.set_content(content, "text/html; charset=utf-8");
        }
        else
        {
            res.status = 404;
            res.set_content("ERROR: record-de-tiro.html not found", "text/plain");
        }
    });

    server.Get("/", [](const httplib::Request&, httplib::Response& res)
    {
        std::ifstream file;

        std::filesystem::path exeDir = getExeDir();

        std::vector<std::filesystem::path> paths = {
            exeDir / "index.html",
            exeDir / "web_ui/index.html",
            exeDir / "../web_ui/index.html",
            exeDir / "../../web_ui/index.html",
            exeDir / "../../../web_ui/index.html",

            exeDir / "backend/web_ui/index.html",
            exeDir / "../backend/web_ui/index.html",
            exeDir / "../../backend/web_ui/index.html"
        };

        for(const auto& p : paths)
        {
            file.open(p);

            if(file.is_open())
            {
                std::cout << "Loaded index from: " << p << std::endl;
                break;
            }
        }

        if(!file.is_open())
        {
            res.set_content("ERROR: index.html not found", "text/plain");
            return;
        }

        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );

        // Sin cache: al recargar la ventana siempre toma el index.html actual
        // (si no, el navegador puede seguir usando una version vieja de la UI).
        res.set_header("Cache-Control", "no-store, must-revalidate");
        res.set_content(content, "text/html");
    });

    std::cout << "Server running on http://127.0.0.1:8071\n";

    server.listen("127.0.0.1", 8071);
}

//////////////////////////////////////////////////
// CONSOLA WINDOWS
//////////////////////////////////////////////////

#ifdef _WIN32

void runConsole()
{
    std::string input;

    std::cout << engine.execute("*") << std::endl;

    while(true)
    {
        std::cout << "> ";
        input.clear();

        while(true)
        {
            int ch = _getch();

            if(ch == 13)
            {
                std::cout << std::endl;
                break;
            }

            if(ch == 8)
            {
                if(!input.empty())
                {
                    input.pop_back();
                    std::cout << "\b \b";
                }

                continue;
            }

            if(ch >= 96 && ch <= 105)
            {
                char c = '0' + (ch - 96);
                input += c;
                std::cout << c;
                continue;
            }

            if(ch >= 48 && ch <= 57)
            {
                char c = (char)ch;
                input += c;
                std::cout << c;
                continue;
            }

            if((ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122))
            {
                char c = (char)ch;
                input += c;
                std::cout << c;
                continue;
            }

            if(ch == 32)
            {
                input += ' ';
                std::cout << ' ';
                continue;
            }
        }

        if(input == "EXIT" || input == "exit")
        {
            std::cout << "Closing console...\n";
            break;
        }

        try
        {
            std::string result = engine.execute(input);
            std::cout << result << std::endl;
        }
        catch(const std::exception& e)
        {
            std::cout << "ERROR: " << e.what() << std::endl;
        }
    }
}

#else

//////////////////////////////////////////////////
// CONSOLA LINUX / WSL
//////////////////////////////////////////////////

void runConsole()
{
    std::string input;

    std::cout << engine.execute("*") << std::endl;

    while(true)
    {
        std::cout << "> ";
        std::getline(std::cin, input);

        if(input == "EXIT" || input == "exit")
        {
            std::cout << "Closing console...\n";
            break;
        }

        try
        {
            std::string result = engine.execute(input);
            std::cout << result << std::endl;
        }
        catch(const std::exception& e)
        {
            std::cout << "ERROR: " << e.what() << std::endl;
        }
    }
}

#endif

//////////////////////////////////////////////////
// MAIN
//////////////////////////////////////////////////

int main()
{
    std::cout << "Starting HP-71B Artillery System...\n";

    engine.resetData();

    std::thread server_thread(runServer);
    std::thread console_thread(runConsole);

    console_thread.join();

    if(server_thread.joinable())
        server_thread.detach();

    return 0;
}