#include <shlwapi.h>
#include <unordered_map>
#include <filesystem>
#include "Memory.h"
#include "MinHook.h"
#include "ini.h"

HMODULE GameModule = 0;
uintptr_t GameBase = 0;

mINI::INIStructure ConfigValues;
bool PreferMovies = false;
bool DisableFilter = false;

uint8_t* MenuState = 0;
uint8_t* MovieType = 0;
uint8_t* MovieCurrentId = 0;
uint32_t* CurrentScript = 0;
uint32_t* SkipScriptFlag = 0;

std::unordered_map<int, int> ScriptVideoMap = 
{
    {6, 9}, // Ovelia's Abduction
    {60, 10}, // Blades of Grass
    {90, 11}, // Partings
    {138, 12}, // Reuinion with Delita
    {172, 13}, // Delita's Warning
    {255, 14}, // Ovelia and Delita
    {345, 15}, // Delita's Will
};

typedef __int64 __fastcall InitMovie_t();
InitMovie_t* InitMovie = 0;

typedef __int64 __fastcall InitScriptVM_t();
InitScriptVM_t* InitScriptVM;
__int64 __fastcall InitScriptVM_Hook()
{
    auto result = InitScriptVM();
    int scriptId = *CurrentScript;
    if (PreferMovies && *MenuState == 0 && ScriptVideoMap.find(scriptId) != ScriptVideoMap.end())
    {
        *SkipScriptFlag = 1;
        *MovieCurrentId = ScriptVideoMap[scriptId];
        *MovieType = 0; // otherwise black screen after movie
        InitMovie();
    }
    return result;
}

void InstallHooks()
{
    MH_Initialize();

    if (PreferMovies)
    {
        uintptr_t initScriptVMOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83 EC 20 E8");
        uintptr_t menuStateOffset = (uintptr_t)Memory::PatternScan(GameModule, "F6 05 ?? ?? ?? ?? 08 0F 85 ?? ?? ?? ?? 8D 5D");
        uintptr_t movieTypeOffset = (uintptr_t)Memory::PatternScan(GameModule, "C7 05 ?? ?? ?? ?? 01 00 00 00 C7 05 ?? ?? ?? ?? 01 00 00 00 83 ?? 10 01");
        uintptr_t movieCurrentOffset = (uintptr_t)Memory::PatternScan(GameModule, "0F B6 05 ?? ?? ?? ?? 3C FF 74 ??");
        uintptr_t currentScriptOffset = (uintptr_t)Memory::PatternScan(GameModule, "81 3D ?? ?? ?? ?? 76 01 00 00");
        uintptr_t scriptSkipFlag = (uintptr_t)Memory::PatternScan(GameModule, "B8 20 00 00 00 44 89 05 ?? ?? ?? ??");
        InitMovie = (InitMovie_t*)(Memory::PatternScan(GameModule, "48 83 EC 30 31 F6 89 74 24 40 E8 ?? ?? ?? ?? 39 35 ?? ?? ?? ?? 74 07") - 0xB);

        if (initScriptVMOffset && menuStateOffset && movieTypeOffset && movieCurrentOffset && currentScriptOffset && scriptSkipFlag && InitMovie)
        {
            int32_t menuStateOffset_Relative = *reinterpret_cast<int32_t*>((uint8_t*)(menuStateOffset + 19));
            MenuState = (uint8_t*)(menuStateOffset + 23) + menuStateOffset_Relative;

            uint8_t* movieType_Relative = (uint8_t*)(movieTypeOffset + 12);
            int32_t movieType_Relative32 = *reinterpret_cast<int32_t*>(movieType_Relative);
            MovieType = (uint8_t*)(movieType_Relative + 8 + movieType_Relative32);

            uint8_t* movieCurrent_Ptr = (uint8_t*)movieCurrentOffset + 3;
            int32_t movieCurrentOffset_Relative = *reinterpret_cast<int32_t*>(movieCurrent_Ptr);
            MovieCurrentId = (uint8_t*)(movieCurrent_Ptr + 4) + movieCurrentOffset_Relative;

            CurrentScript = (uint32_t*)(currentScriptOffset + 10 + *reinterpret_cast<int32_t*>(currentScriptOffset + 2));
            SkipScriptFlag = (uint32_t*)(scriptSkipFlag + 12 + *reinterpret_cast<int32_t*>(scriptSkipFlag + 8));

            Memory::DetourFunction(initScriptVMOffset, (LPVOID)InitScriptVM_Hook, (LPVOID*)&InitScriptVM);
        }
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

    mINI::INIFile ini("scripts/FFTacticsFix.ini"); // todo: robust ini loading and logging
    ini.read(ConfigValues);
    PreferMovies = std::stoi(ConfigValues["Settings"]["PreferMovies"]) > 0;
    DisableFilter = std::stoi(ConfigValues["Settings"]["DisableFilter"]) > 0;

    Sleep(5000); // todo: proper ASI init

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

