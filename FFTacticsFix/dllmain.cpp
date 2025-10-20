#include <shlwapi.h>
#include <unordered_map>
#include <filesystem>
#include "Memory.h"
#include "MinHook.h"
#include "ini.h"
#include "logger.h"
#include "config.h";

HMODULE GameModule = 0;
uintptr_t GameBase = 0;

mINI::INIStructure ConfigValues;
GameConfig Config;

const int BASE_WIDTH = 1920;
const int BASE_HEIGHT = 1080;
const float BASE_ASPECT = 16.0f / 9.0f;

int* FBO_W = 0;
int* FBO_H = 0;
float* InternalRenderScale = 0;
float* ClipScale = 0;
uint8_t* MenuState = 0;
uint8_t* MovieType = 0;
int8_t* MovieCurrentId = 0;
uint32_t* CurrentScript = 0;
uint32_t* SkipScriptFlag = 0;
int* m_iScreenW = 0;
int* m_iScreenH = 0;
int* gEventOrBattle = 0;
uintptr_t GraphicsManager = 0;

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

inline float GetAspectRatio()
{
    if (!m_iScreenW || !m_iScreenH)
        return BASE_ASPECT;

    return *m_iScreenW / *m_iScreenH;
}

inline int GetScaledFBOWidth(float renderScaleFactor) 
{
    int width = m_iScreenW ? *m_iScreenW : BASE_WIDTH;
    int height = m_iScreenH ? *m_iScreenH : BASE_HEIGHT;

    float targetAspect = (float)width / (float)height;
    float aspectCorrection = targetAspect <= BASE_ASPECT ? 1.0f : targetAspect / BASE_ASPECT;
    return (int)(BASE_WIDTH * aspectCorrection * renderScaleFactor);
}

inline int GetScaledFBOHeight(float renderScaleFactor) 
{
    return (int)(BASE_HEIGHT * renderScaleFactor);
}

typedef __int64 __fastcall InitMovie_t();
InitMovie_t* InitMovie = 0;

typedef __int64 __fastcall InitScriptVM_t();
InitScriptVM_t* InitScriptVM;
__int64 __fastcall InitScriptVM_Hook()
{
    auto result = InitScriptVM();
    int scriptId = *CurrentScript;
    if (Config.PreferMovies && *MenuState == 0 && ScriptVideoMap.find(scriptId) != ScriptVideoMap.end())
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
    char movieId = SetMovieId();

    if (FBO_W && FBO_H)
    {
        if (movieId > 0)
        {
            *FBO_W = BASE_WIDTH;
            *FBO_H = BASE_HEIGHT;
        }
        else
        {
            float factor = Config.RenderScale / 4.0f;
            *FBO_W = GetScaledFBOWidth(factor);
            *FBO_H = GetScaledFBOHeight(factor);
        }
    }

    return movieId;
}

typedef void __fastcall CFFT_STATE__SetRenderSize_t(__int64 a1, int width, int height);
CFFT_STATE__SetRenderSize_t* CFFT_STATE__SetRenderSize;
void __fastcall CFFT_STATE__SetRenderSize_Hook(__int64 a1, int width, int height)
{
    float factor = Config.RenderScale / 4.0f;
    *FBO_W = GetScaledFBOWidth(factor);
    *FBO_H = GetScaledFBOHeight(factor);

    *InternalRenderScale = (float)Config.RenderScale;

    CFFT_STATE__SetRenderSize(a1, width, height);

    *ClipScale = 1.0f * (*InternalRenderScale / 4.0);
}

typedef int* __fastcall CalculateViewportWithLetterboxing_t(int* a1, int a2, int a3, char a4);
CalculateViewportWithLetterboxing_t* CalculateViewportWithLetterboxing;
int* __fastcall CalculateViewportWithLetterboxing_Hook(int* outRect, int contentWidth, int contentHeight, char preserveWidth)
{
    float currentAspect = GetAspectRatio();
    if (currentAspect <= BASE_ASPECT)
        return CalculateViewportWithLetterboxing(outRect, contentWidth, contentHeight, preserveWidth);

    int letterboxOffsetX = 0;
    int letterboxOffsetY = 0;

    int screenWidth = *m_iScreenW;
    int screenHeight = *m_iScreenH;

    float scale = (float)screenHeight / (float)contentHeight;

    int scaledHeight = (int)((float)contentHeight * scale);
    int scaledWidth;

    if (!preserveWidth)
        scaledWidth = (int)((float)contentWidth * scale);
    else
        scaledWidth = screenWidth;

    int viewportX = (screenWidth - scaledWidth) / 2 + letterboxOffsetX;
    int viewportY = (screenHeight - scaledHeight) / 2 + letterboxOffsetY;

    outRect[0] = viewportX; // left
    outRect[1] = viewportY; // top
    outRect[2] = viewportX + scaledWidth; // right
    outRect[3] = viewportY + scaledHeight; // bottom

    return outRect;
}

typedef char __fastcall UILayerUpdate_t(__int64 layer);
UILayerUpdate_t* UILayerUpdate;
char __fastcall UILayerUpdate_Hook(__int64 layer)
{
    float currentAspect = GetAspectRatio();
    if (currentAspect <= BASE_ASPECT)
        return UILayerUpdate(layer);

    auto uiRenderer = *(__int64*)(*(uintptr_t*)GraphicsManager + 0x9430);
    auto firstLayer = *(__int64*)(uiRenderer + 0xD0);

    if (layer == firstLayer)
        return 0; // hide pillarbox

    return UILayerUpdate(layer);
}

typedef float* __fastcall SetupCameraCenter_t(__int64 a1, __int64 a2);
SetupCameraCenter_t* SetupCameraCenter;
float* __fastcall SetupCameraCenter_Hook(__int64 a1, __int64 a2)
{
    float currentAspect = GetAspectRatio();
    if (currentAspect <= BASE_ASPECT)
        return SetupCameraCenter(a1, a2);;

    float originalX = *(float*)(a2 + 120);
    float aspectDelta = currentAspect - BASE_ASPECT;

    float adjustment = aspectDelta * 105.0f;
    *(float*)(a2 + 120) = originalX + adjustment;
    float* result = SetupCameraCenter(a1, a2);
    *(float*)(a2 + 120) = originalX;

    return result;
}

typedef void __fastcall WorldToScreen_t(float* a1, float* a2, float* a3);
WorldToScreen_t* WorldToScreen;
void __fastcall WorldToScreen_Hook(float* a1, float* a2, float* a3)
{
    WorldToScreen(a1, a2, a3);

    float currentAspect = GetAspectRatio();
    if (currentAspect <= BASE_ASPECT)
        return;

    float factor = Config.RenderScale / 4.0f;
    float scaledBaseWidth = BASE_WIDTH * factor;
    float fboWidthDelta = (float)(*FBO_W) - scaledBaseWidth;
    float offset = (fboWidthDelta * -0.0975f) / factor;
    a2[0] = a2[0] + offset;
}

typedef __int64 __fastcall SetScreenSize_t(__int64 a1, int width, int height);
SetScreenSize_t* SetScreenSize;
__int64 __fastcall SetScreenSize_Hook(__int64 a1, int width, int height)
{
    spdlog::info("SetScreenSize: {}x{}", width, height);

    auto result = SetScreenSize(a1, width, height);

    float factor = Config.RenderScale / 4.0f;
    *FBO_W = GetScaledFBOWidth(factor);
    *FBO_H = GetScaledFBOHeight(factor);

    return result;
}

void PatchResolution()
{
    uintptr_t fboOffset = (uintptr_t)Memory::PatternScan(GameModule, "C7 05 ?? ?? ?? ?? 38 04 00 00");
    uintptr_t resScaleOffset = (uintptr_t)Memory::PatternScan(GameModule, "0F 2F 05 ?? ?? ?? ?? 72 ?? 44 88 ?? 26");
    uintptr_t setRenderSizeOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 ?? ?? 48 89 58 08 48 89 ?? 10 ?? 48 83 EC ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? 0F 29 78");
    uintptr_t clipScaleOffset = (uintptr_t)Memory::PatternScan(GameModule, "83 3D ?? ?? ?? ?? 07 C7 05 ?? ?? ?? ?? 00 00 80 3F");

    if (fboOffset && resScaleOffset && setRenderSizeOffset && clipScaleOffset)
    {
        int32_t fboRelative = *(int32_t*)(fboOffset + 2);
        uintptr_t fboHAddress = fboOffset + 10 + fboRelative;

        FBO_W = (int*)(fboHAddress - 4);
        FBO_H = (int*)(fboHAddress);

        int32_t resScale_Relative = *(int32_t*)(resScaleOffset + 3);
        InternalRenderScale = (float*)(resScaleOffset + 7 + resScale_Relative);

        int32_t clipScaleRelative = *(int32_t*)(clipScaleOffset + 9);
        ClipScale = (float*)(clipScaleOffset + 7 + 10 + clipScaleRelative);

        DWORD oldProtect;
        VirtualProtect((LPVOID)(fboOffset - 0xA), 6, PAGE_EXECUTE_READWRITE, &oldProtect);
        memset((void*)(fboOffset - 0xA), 0x90, 6);

        VirtualProtect((LPVOID)(fboOffset), 0xA, PAGE_EXECUTE_READWRITE, &oldProtect);
        memset((void*)(fboOffset), 0x90, 0xA);

        VirtualProtect((LPVOID)(InternalRenderScale), 4, PAGE_READWRITE, &oldProtect);

        float factor = Config.RenderScale / 4.0f;
        *FBO_W = (int)(BASE_WIDTH * factor);
        *FBO_H = (int)(BASE_HEIGHT * factor);

        spdlog::info("FBO: {}x{}", *FBO_W, *FBO_H);

        Memory::DetourFunction(setRenderSizeOffset, (LPVOID)CFFT_STATE__SetRenderSize_Hook, (LPVOID*)&CFFT_STATE__SetRenderSize);
    }

    spdlog::debug("fboOffset: {:#x}", fboOffset);
    spdlog::debug("resScaleOffset: {:#x}", resScaleOffset);
    spdlog::debug("setRenderSizeOffset: {:#x}", setRenderSizeOffset);
    spdlog::debug("clipScaleOffset: {:#x}", clipScaleOffset);
}

void PatchViewport()
{
    uintptr_t m_iScreenWOffset = (uintptr_t)Memory::PatternScan(GameModule, "BE 80 07 00 00 89 ?? ?? ?? ?? ??");

    if (!m_iScreenWOffset)
    {
        spdlog::error("Failed to find screen width offset");
        return;
    }

    uintptr_t m_iScreenHOffset = (uintptr_t)Memory::PatternScan(GameModule, "B9 E0 04 00 00 89 ?? ?? ?? ?? ??");
    if (!m_iScreenHOffset)
    {
        spdlog::error("Failed to find screen height offset");
        return;
    }

    m_iScreenW = (int*)(m_iScreenWOffset + 11 + *(int32_t*)(m_iScreenWOffset + 7));
    m_iScreenH = (int*)(m_iScreenHOffset + 11 + *(int32_t*)(m_iScreenHOffset + 7));

    spdlog::info("Screen Dimensions: {}x{}", *m_iScreenW, *m_iScreenH);

    uintptr_t setupCamCenterOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 8B C4 55 48 8D 68");
    uintptr_t world2ScreenOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 8B C4 48 89 58 18 48 89 78 20 55 48 8D 68 ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 44 0F 29 48 ??");
    uintptr_t calcViewportOffset = (uintptr_t)(Memory::PatternScan(GameModule, "89 78 20 41 56 41 57 48 8B 05") - 0x10);
    uintptr_t rightPlaneClipOffset = (uintptr_t)Memory::PatternScan(GameModule, "00 80 E2 43");
    uintptr_t leftPlaneClipOffset = (uintptr_t)Memory::PatternScan(GameModule, "00 00 6C 42 00 00 70 42");
    uintptr_t layerUpdateOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 8B C4 55 53 56 57 48");
    uintptr_t graphicsManagerOffset = (uintptr_t)Memory::PatternScan(GameModule, "48 8B 05 ?? ?? ?? ?? ?? 8B ?? 30 94 00 00");

    if (setupCamCenterOffset && world2ScreenOffset && calcViewportOffset && rightPlaneClipOffset && 
        leftPlaneClipOffset && layerUpdateOffset && graphicsManagerOffset)
    {
        DWORD oldProtect;
        VirtualProtect((LPVOID)(leftPlaneClipOffset), 4, PAGE_READWRITE, &oldProtect);
        VirtualProtect((LPVOID)(rightPlaneClipOffset), 4, PAGE_READWRITE, &oldProtect);

        *(float*)(leftPlaneClipOffset) = *(float*)(leftPlaneClipOffset) * -2;
        *(float*)(rightPlaneClipOffset) = *(float*)(rightPlaneClipOffset) * 2;

        GraphicsManager = (uintptr_t)(graphicsManagerOffset + *(int*)(graphicsManagerOffset + 3) + 7);

        Memory::DetourFunction(calcViewportOffset, (LPVOID)CalculateViewportWithLetterboxing_Hook, (LPVOID*)&CalculateViewportWithLetterboxing);
        Memory::DetourFunction(layerUpdateOffset, (LPVOID)UILayerUpdate_Hook, (LPVOID*)&UILayerUpdate);
        Memory::DetourFunction(setupCamCenterOffset, (LPVOID)SetupCameraCenter_Hook, (LPVOID*)&SetupCameraCenter);
        Memory::DetourFunction(world2ScreenOffset, (LPVOID)WorldToScreen_Hook, (LPVOID*)&WorldToScreen);
    }

    spdlog::debug("setupCamCenterOffset: {:#x}", setupCamCenterOffset);
    spdlog::debug("world2ScreenOffset: {:#x}", world2ScreenOffset);
    spdlog::debug("calcViewportOffset: {:#x}", calcViewportOffset);
    spdlog::debug("rightPlaneClipOffset: {:#x}", rightPlaneClipOffset);
    spdlog::debug("leftPlaneClipOffset: {:#x}", leftPlaneClipOffset);
    spdlog::debug("layerUpdateOffset: {:#x}", layerUpdateOffset);
    spdlog::debug("graphicsManagerOffset: {:#x}", GraphicsManager);
}

// pattern matching has gotten out of hand. 
// possibly better to hardcode offsets based on exe version
void ApplyPatches()
{
    uintptr_t movieCurrentOffset = (uintptr_t)Memory::PatternScan(GameModule, "0F B6 05 ?? ?? ?? ?? 3C FF 74 ??");
    uint8_t* movieCurrentPtr = (uint8_t*)movieCurrentOffset + 3;
    int32_t movieCurrentOffsetRelative = *reinterpret_cast<int32_t*>(movieCurrentPtr);
    MovieCurrentId = (int8_t*)(movieCurrentPtr + 4) + movieCurrentOffsetRelative;

    if (movieCurrentOffset)
    {
        Memory::DetourFunction(movieCurrentOffset, (LPVOID)SetMovieId_Hook, (LPVOID*)&SetMovieId);
    }

    spdlog::debug("movieCurrentOffset: {:#x}", movieCurrentOffset);

    if (Config.DisableFilter)
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

        spdlog::debug("grainFilterOffset: {:#x}", grainFilterOffset);
        spdlog::debug("pauseMenuViewportResizeOffset: {:#x}", pauseMenuViewportResizeOffset);
    }

    if (Config.PreferMovies)
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

        spdlog::debug("initScriptVMOffset: {:#x}", initScriptVMOffset);
        spdlog::debug("menuStateOffset: {:#x}", menuStateOffset);
        spdlog::debug("movieTypeOffset: {:#x}", menuStateOffset);
        spdlog::debug("currentScriptOffset: {:#x}", menuStateOffset);
        spdlog::debug("scriptSkipFlag: {:#x}", menuStateOffset);
    }
}

typedef void GX_Init_t();
GX_Init_t* GX_Init;
void GX_Init_Hook()
{
    PatchResolution();
    GX_Init();
    PatchViewport();
    ApplyPatches();
}

void Init()
{
    spdlog::info("Installing hooks and applying patches...");

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK)
    {
        spdlog::error("Failed to initialize MinHook, status: {}", (int)status);
        return;
    }

    uintptr_t gxInitOffset = (uintptr_t)(Memory::PatternScan(GameModule, "ED B9 30 18 00 00") - 0x3A);
    uintptr_t setScreenSizeOffset = (uintptr_t)(Memory::PatternScan(GameModule, "48 39 ?? 58 75") - 0xB);

    if (!gxInitOffset)
    {
        spdlog::error("Unable to find GX_Init!");
        return;
    }

    Memory::DetourFunction(gxInitOffset, (LPVOID)GX_Init_Hook, (LPVOID*)&GX_Init);

    if (!setScreenSizeOffset)
    {
        spdlog::error("Unable to find SetScreenSize!");
        return;
    }

    Memory::DetourFunction(setScreenSizeOffset, (LPVOID)SetScreenSize_Hook, (LPVOID*)&SetScreenSize);
}

void ReadConfig()
{
    std::string configPath = "scripts/FFTacticsFix.ini";
    mINI::INIFile ini(configPath);
    if (!ini.read(ConfigValues))
    {
        ConfigValues["Settings"]["PreferMovies"] = "0";
        ConfigValues["Settings"]["DisableFilter"] = "0";
        ConfigValues["Settings"]["RenderScale"] = "4";
        ini.generate(ConfigValues);
    }

    auto readConfigInt = [](const std::string& str, int defaultValue) -> int {
        if (str.empty()) return defaultValue;
        try { return std::stoi(str); }
        catch (...) { return defaultValue; }
    };

    Config.PreferMovies = readConfigInt(ConfigValues["Settings"]["PreferMovies"], 0);
    Config.DisableFilter = readConfigInt(ConfigValues["Settings"]["DisableFilter"], 0);
    Config.RenderScale = readConfigInt(ConfigValues["Settings"]["RenderScale"], 4);
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    GameModule = GetModuleHandleA("FFT_enhanced.exe");
    GameBase = (uintptr_t)GameModule;

    if (GameModule == 0)
        return true;

    WCHAR exePath[_MAX_PATH] = { 0 };
    GetModuleFileNameW(GameModule, exePath, MAX_PATH);
    std::filesystem::path sExePath = exePath;
    WCHAR* filename = PathFindFileName(exePath);

    if (wcsncmp(filename, L"FFT_enhanced.exe", 16) != 0)
        return true;

    ReadConfig();

    if (!InitializeLogger(GameModule, sExePath))
        return true;

    Init();

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

