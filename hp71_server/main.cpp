#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <filesystem>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
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
// SERVER THREAD
//////////////////////////////////////////////////

void runServer()
{
    httplib::Server server;

    server.Post("/input", [](const httplib::Request& req, httplib::Response& res)
    {
        std::string cmd = req.get_param_value("cmd");
        std::cout << "[INPUT_HTTP] [" << cmd << "]" << std::endl;
        std::string result = engine.execute(cmd);
        res.set_content(result, "text/plain");
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

        res.set_content(content, "text/html");
    });

    std::cout << "Server running on http://127.0.0.1:8080\n";

    server.listen("127.0.0.1", 8080);
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

#ifndef _WIN32
    if(!isatty(STDIN_FILENO))
    {
        runServer();
        return 0;
    }
#endif

    std::thread server_thread(runServer);
    std::thread console_thread(runConsole);

    console_thread.join();

    if(server_thread.joinable())
        server_thread.detach();

    return 0;
}
