#include <iostream>
#include <fstream>
#include <string>
#include <Windows.h>
#include <shlobj.h>
#include <thread>
#include <chrono>
#include "json.hpp"
#include <psapi.h>
#include <TlHelp32.h>
#include <cstdlib>
#include <filesystem>
#include <tchar.h>
using json = nlohmann::json;

std::string wstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &strTo[0], sizeNeeded, nullptr, nullptr);
    return strTo;
}

std::wstring utf8ToWstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstrTo[0], sizeNeeded);
    return wstrTo;
}

std::wstring OpenFileDialog(const wchar_t* title) {
    HWND consoleWindow = GetConsoleWindow();
    ShowWindow(consoleWindow, SW_HIDE);

    wchar_t filename[MAX_PATH];
    OPENFILENAME ofn;
    ZeroMemory(&filename, sizeof(filename));
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"Executable Files\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = title;

    std::wstring filePath;
    if (GetOpenFileName(&ofn)) {
        filePath = std::wstring(filename);
    }

    ShowWindow(consoleWindow, SW_SHOW);

    return filePath;
}

void SavePathsToJson(const std::wstring& migotoPath, const std::wstring& genshinPath) {
    json paths;
    paths["3dmigoto"] = wstringToUtf8(migotoPath);
    paths["Genshin"] = wstringToUtf8(genshinPath);

    wchar_t buffer[MAX_PATH];
    GetModuleFileName(nullptr, buffer, MAX_PATH);
    std::wstring exePath(buffer);
    std::wstring jsonPath = exePath.substr(0, exePath.find_last_of(L"\\") + 1) + L"paths.json";

    std::ofstream outputFile(jsonPath);
    if (!outputFile.is_open()) {
        std::wcerr << L"Failed to open paths.json for writing.\n";
        char buffer[256];
        strerror_s(buffer, 256, errno);
        std::wcerr << buffer << "\n";
        return;
    }

    outputFile << paths.dump(4);
    outputFile.close();
}

bool ReadPathsFromJson(std::wstring& migotoPath, std::wstring& genshinPath) {
    wchar_t buffer[MAX_PATH];
    GetModuleFileName(nullptr, buffer, MAX_PATH);
    std::wstring exePath(buffer);
    std::wstring jsonPath = exePath.substr(0, exePath.find_last_of(L"\\") + 1) + L"paths.json";

    std::ifstream inputFile(jsonPath);
    if (!inputFile.is_open()) {
        std::wcerr << L"Failed to open file.\n";
        return false;
    }

    json paths;
    try {
        inputFile >> paths;
        migotoPath = utf8ToWstring(paths["3dmigoto"].get<std::string>());
        genshinPath = utf8ToWstring(paths["Genshin"].get<std::string>());
    }
    catch (json::parse_error& e) {
        std::wcerr << L"Failed to parse paths.json file: " << e.what() << "\n";
        return false;
    }

    return true;
}

void kill_process(const wchar_t* process_name)
{
    wchar_t cmd_line[MAX_PATH + 30];
    _stprintf_s(cmd_line, L"taskkill /f /im %s", process_name);

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcess(NULL, cmd_line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        std::wcerr << L"Failed to execute taskkill command for: " << process_name << std::endl;
    }
    else
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int main()
{
    kill_process(L"GenshinImpact.exe");

    std::wstring migotoPath, genshinPath;

    if (!ReadPathsFromJson(migotoPath, genshinPath)) {
        migotoPath = OpenFileDialog(L"Select the 3DMigoto executable");
        if (migotoPath.empty()) {
            std::wcerr << L"No 3DMigoto file selected. Exiting.\n";
            return 1;
        }

        genshinPath = OpenFileDialog(L"Select Genshin Impact game executable");
        if (genshinPath.empty()) {
            std::wcerr << L"No Genshin Impact file selected. Exiting.\n";
            return 1;
        }

        SavePathsToJson(migotoPath, genshinPath);
    }

    std::wstring migotoDir = migotoPath.substr(0, migotoPath.find_last_of(L"\\") + 1);

    STARTUPINFO siMigoto = { sizeof(siMigoto) };
    PROCESS_INFORMATION piMigoto;
    BOOL bMigotoCreated = CreateProcess(nullptr, const_cast<wchar_t*>(migotoPath.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &siMigoto, &piMigoto);
    if (!bMigotoCreated) {
        std::wcerr << L"Failed to start 3DMigoto process.\n";
        return 1;
    }
    CloseHandle(piMigoto.hThread);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    STARTUPINFO siGenshin = { sizeof(siGenshin) };

    PROCESS_INFORMATION piGenshin;
    wchar_t cmd_line[MAX_PATH + 30];
    BOOL bGenshinCreated = CreateProcess(nullptr, const_cast<wchar_t*>(genshinPath.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &siGenshin, &piGenshin);
    if (!bGenshinCreated) {
        std::wcerr << L"Failed to start Genshin Impact process.\n";
        return 1;
    }
    CloseHandle(piGenshin.hThread);
    return 0;
}