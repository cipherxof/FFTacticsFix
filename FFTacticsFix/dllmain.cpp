#include <shlwapi.h>
#include <unordered_map>
#include "Memory.h"
#include "MinHook.h"
#include "ini.h"
#include <filesystem>

HMODULE GameModule = 0;
uintptr_t GameBase = 0;

mINI::INIStructure ConfigValues;
bool PreferMovies = false;
bool DisableFilter = false;

uint8_t* MenuState = 0;
uint8_t* MovieType = 0;
uint8_t* MovieCurrentId = 0;

std::unordered_map<int, int> scriptVideoMap = 
{
    {30, 9}, // Ovelia's Abduction
    {300, 10}, // Blades of Grass
    {450, 11}, // Partings
    {690, 12}, // Reuinion with Delita
    {860, 13} // Delita's Warning
};

typedef __int64 __fastcall InitMovie_t();
InitMovie_t* InitMovie = 0;

typedef char __fastcall PlayCutscene_t(unsigned int a1, __int64 a2);
PlayCutscene_t* PlayScript;
char __fastcall PlayScript_Hook(unsigned int sceneId, __int64 a2)
{
    if (PreferMovies && *MenuState == 0 && scriptVideoMap.find(sceneId) != scriptVideoMap.end()) 
    {
        *MovieCurrentId = scriptVideoMap[sceneId];
        *MovieType = 0;
        InitMovie();
        return 1;
    }

    return PlayScript(sceneId, a2);
}

void InstallHooks()
{
    MH_Initialize();

    uintptr_t playScriptOffset = (uintptr_t)Memory::PatternScan(GameModule, "83 FB 11 75 ?? 48 89 F2 89 F9 E8 ?? ?? ?? ??");
    uintptr_t menuStateOffset = (uintptr_t)Memory::PatternScan(GameModule, "F6 05 ?? ?? ?? ?? 08 0F 85 ?? ?? ?? ?? 8D 5D");
    uintptr_t movieTypeOffset = (uintptr_t)Memory::PatternScan(GameModule, "C7 05 ?? ?? ?? ?? 01 00 00 00 C7 05 ?? ?? ?? ?? 01 00 00 00 83 ?? 10 01");
    uintptr_t movieCurrentOffset = (uintptr_t)Memory::PatternScan(GameModule, "0F B6 05 ?? ?? ?? ?? 3C FF 74 ??");
    InitMovie = (InitMovie_t*)(Memory::PatternScan(GameModule, "48 83 EC 30 31 F6 89 74 24 40 E8 ?? ?? ?? ?? 39 35 ?? ?? ?? ?? 74 07") - 0xB);

    if (playScriptOffset && menuStateOffset && movieTypeOffset && movieCurrentOffset && InitMovie) 
    {
        uint8_t* playScriptOffset_Call = (uint8_t*)(playScriptOffset + 10);
        int32_t playScriptOffset_Relative = *reinterpret_cast<int32_t*>(playScriptOffset_Call + 1);
        uintptr_t playScriptOffset_Absolute = (uintptr_t)(playScriptOffset_Call + 5 + playScriptOffset_Relative);

        int32_t menuStateOffset_Relative = *reinterpret_cast<int32_t*>((uint8_t*)(menuStateOffset + 19));
        MenuState = (uint8_t*)(menuStateOffset + 23) + menuStateOffset_Relative;

        uint8_t* movieType_Relative = (uint8_t*)(movieTypeOffset + 12);
        int32_t movieType_Relative32 = *reinterpret_cast<int32_t*>(movieType_Relative);
        MovieType = (uint8_t*)(movieType_Relative + 8 + movieType_Relative32);

        uint8_t* movieCurrent_Ptr = (uint8_t*)movieCurrentOffset + 3;
        int32_t movieCurrentOffset_Relative = *reinterpret_cast<int32_t*>(movieCurrent_Ptr);
        MovieCurrentId = (uint8_t*)(movieCurrent_Ptr + 4) + movieCurrentOffset_Relative;

        Memory::DetourFunction(playScriptOffset_Absolute, (LPVOID)PlayScript_Hook, (LPVOID*)&PlayScript);
    }
}

void ApplyPatches()
{
    if (DisableFilter)
    {
        uintptr_t grainFilterOffset = (uintptr_t)Memory::PatternScan(GameModule, "38 ?? A0 55 02 00");
        uintptr_t pauseMenuViewportResizeOffset = (uintptr_t)Memory::PatternScan(GameModule, "80 ?? B5 55 02 00 00");

        if (grainFilterOffset) 
        {
            DWORD oldProtect;
            VirtualProtect((LPVOID)(grainFilterOffset), 6, PAGE_EXECUTE_READWRITE, &oldProtect);
            memset((void*)grainFilterOffset, 0x90, 6);
        }

        if (pauseMenuViewportResizeOffset) 
        {
            DWORD oldProtect;
            VirtualProtect((LPVOID)(pauseMenuViewportResizeOffset), 7, PAGE_EXECUTE_READWRITE, &oldProtect);
            memset((void*)pauseMenuViewportResizeOffset, 0x90, 7);
        }
    }
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    GameModule = GetModuleHandleA("FFT_enhanced.exe");
    GameBase = (uintptr_t)GameModule;

    if (GameModule == 0)
        return true;

    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileName(GameModule, exePath, MAX_PATH);
    WCHAR* filename = PathFindFileName(exePath);

    if (wcsncmp(filename, L"FFT_enhanced.exe", 16) != 0)
        return true;

    if (!std::filesystem::exists("scripts/FFTacticsFix.ini"))
        return true;

    mINI::INIFile ini("scripts/FFTacticsFix.ini");
    ini.read(ConfigValues);
    PreferMovies = std::stoi(ConfigValues["Settings"]["PreferMovies"]) > 0;
    DisableFilter = std::stoi(ConfigValues["Settings"]["DisableFilter"]) > 0;

    Sleep(5000);

    InstallHooks();
    ApplyPatches();

    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, NULL, MainThread, NULL, NULL, NULL);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

