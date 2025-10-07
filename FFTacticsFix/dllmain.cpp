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
int RenderScale = 10;
int* FBO_W = 0;
int* FBO_H = 0;
float* InternalRenderScale = 0;
float* ClipScale = 0;

uint8_t* MenuState = 0;
uint8_t* MovieType = 0;
int8_t* MovieCurrentId = 0;
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

typedef char __fastcall SetMovieId_t();
SetMovieId_t* SetMovieId;
char __fastcall SetMovieId_Hook()
{
    auto r = SetMovieId();

    if (FBO_W && FBO_H && r > 0)
    {
        *FBO_W = 1920;
        *FBO_H = 1080;
    }

    return r;
}

typedef void __fastcall CFFT_STATE__SetRenderSize_t(__int64 a1, int width, int height);
CFFT_STATE__SetRenderSize_t* CFFT_STATE__SetRenderSize;
void __fastcall CFFT_STATE__SetRenderSize_Hook(__int64 a1, int width, int height)
{
    float factor = RenderScale / 4.0f;
    *FBO_W = (int)(1920 * factor);
    *FBO_H = (int)(1080 * factor);

    *InternalRenderScale = (float)RenderScale;

    CFFT_STATE__SetRenderSize(a1, width, height);

    *ClipScale = 1.0f * (*InternalRenderScale / 4.0);
}

void ApplyPatches()
{
    MH_Initialize();

    bool applyRenderScale = RenderScale > 0 && RenderScale != 4;

    if (applyRenderScale || PreferMovies)
    {
        uintptr_t movieCurrentOffset = (uintptr_t)Memory::PatternScan(GameModule, "0F B6 05 ?? ?? ?? ?? 3C FF 74 ??");
        uint8_t* movieCurrentPtr = (uint8_t*)movieCurrentOffset + 3;
        int32_t movieCurrentOffsetRelative = *reinterpret_cast<int32_t*>(movieCurrentPtr);
        MovieCurrentId = (int8_t*)(movieCurrentPtr + 4) + movieCurrentOffsetRelative;
        Memory::DetourFunction(movieCurrentOffset, (LPVOID)SetMovieId_Hook, (LPVOID*)&SetMovieId);
    }

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

    if (PreferMovies)
    {
        uintptr_t initScriptVMOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83 EC 20 E8");
        uintptr_t menuStateOffset = (uintptr_t)Memory::PatternScan(GameModule, "F6 05 ?? ?? ?? ?? 08 0F 85 ?? ?? ?? ?? 8D 5D");
        uintptr_t movieTypeOffset = (uintptr_t)Memory::PatternScan(GameModule, "C7 05 ?? ?? ?? ?? 01 00 00 00 C7 05 ?? ?? ?? ?? 01 00 00 00 83 ?? 10 01");
        uintptr_t currentScriptOffset = (uintptr_t)Memory::PatternScan(GameModule, "81 3D ?? ?? ?? ?? 76 01 00 00");
        uintptr_t scriptSkipFlag = (uintptr_t)Memory::PatternScan(GameModule, "B8 20 00 00 00 44 89 05 ?? ?? ?? ??");
        InitMovie = (InitMovie_t*)(Memory::PatternScan(GameModule, "48 83 EC 30 31 F6 89 74 24 40 E8 ?? ?? ?? ?? 39 35 ?? ?? ?? ?? 74 07") - 0xB);

        if (initScriptVMOffset && menuStateOffset && movieTypeOffset && MovieCurrentId && currentScriptOffset && scriptSkipFlag && InitMovie)
        {
            int32_t menuStateOffsetRelative = *reinterpret_cast<int32_t*>((uint8_t*)(menuStateOffset + 19));
            MenuState = (uint8_t*)(menuStateOffset + 23) + menuStateOffsetRelative;

            uint8_t* movieTypeRelative = (uint8_t*)(movieTypeOffset + 12);
            int32_t movieTypeRelative32 = *reinterpret_cast<int32_t*>(movieTypeRelative);
            MovieType = (uint8_t*)(movieTypeRelative + 8 + movieTypeRelative32);

            CurrentScript = (uint32_t*)(currentScriptOffset + 10 + *reinterpret_cast<int32_t*>(currentScriptOffset + 2));
            SkipScriptFlag = (uint32_t*)(scriptSkipFlag + 12 + *reinterpret_cast<int32_t*>(scriptSkipFlag + 8));

            Memory::DetourFunction(initScriptVMOffset, (LPVOID)InitScriptVM_Hook, (LPVOID*)&InitScriptVM);
        }
    }

    if (applyRenderScale)
    {
        uintptr_t fboOffset = (uintptr_t)Memory::PatternScan(GameModule, "C7 05 ?? ?? ?? ?? 38 04 00 00");
        uintptr_t resScaleOffset = (uintptr_t)Memory::PatternScan(GameModule, "0F 2F 05 ?? ?? ?? ?? 72 ?? 44 88 ?? 26");
        uintptr_t setRenderSizeOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 8B C4 48 89 58 08 48 89 68 10 56 48");
        uintptr_t clipScaleOffset = (uintptr_t)Memory::PatternScan(GameModule, "83 3D ?? ?? ?? ?? 07 C7 05 ?? ?? ?? ?? 00 00 80 3F");

        if (fboOffset && resScaleOffset)
        {
            int32_t fboRelative = *(int32_t*)(fboOffset + 2);
            uintptr_t fboHAddress = fboOffset + 10 + fboRelative;

            FBO_W = (int*)(fboHAddress - 4);
            FBO_H = (int*)(fboHAddress);

            int32_t resScale_Relative = *(int32_t*)(resScaleOffset + 3);
            InternalRenderScale = (float*)(resScaleOffset + 7 + resScale_Relative);

            int32_t clipScaleRelative = *(int32_t*)(clipScaleOffset + 9);
            ClipScale = (float*)(clipScaleOffset + 7 + 10 + clipScaleRelative);

            float factor = RenderScale / 4.0f;

            *FBO_W = (int)(1920 * factor);
            *FBO_H = (int)(1080 * factor);

            DWORD oldProtect;
            VirtualProtect((LPVOID)(fboOffset - 0xA), 6, PAGE_EXECUTE_READWRITE, &oldProtect);
            memset((void*)(fboOffset - 0xA), 0x90, 6);

            VirtualProtect((LPVOID)(fboOffset), 0xA, PAGE_EXECUTE_READWRITE, &oldProtect);
            memset((void*)(fboOffset), 0x90, 0xA);

            VirtualProtect((LPVOID)(InternalRenderScale), 4, PAGE_READWRITE, &oldProtect);

            Memory::DetourFunction(setRenderSizeOffset, (LPVOID)CFFT_STATE__SetRenderSize_Hook, (LPVOID*)&CFFT_STATE__SetRenderSize);
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

    try {
        mINI::INIFile ini("scripts/FFTacticsFix.ini"); // todo: robust ini loading and logging
        ini.read(ConfigValues);
        PreferMovies = std::stoi(ConfigValues["Settings"]["PreferMovies"]) > 0;
        DisableFilter = std::stoi(ConfigValues["Settings"]["DisableFilter"]) > 0;
        RenderScale = std::stoi(ConfigValues["Settings"]["RenderScale"]);
    }
    catch (...) {
        return true;
    }

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

