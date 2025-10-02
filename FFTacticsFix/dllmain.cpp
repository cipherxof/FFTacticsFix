#include <shlwapi.h>
#include "Memory.h"
#include "MinHook.h"

HMODULE GameModule = 0;
uintptr_t GameBase = 0;

void InstallHooks()
{
    //MH_Initialize();
}

void ApplyPatches()
{
    uintptr_t grainFilterOffset = (uintptr_t)Memory::PatternScan(GameModule, "38 ?? A0 55 02 00");
    uintptr_t pauseMenuViewportResizeOffset = (uintptr_t)Memory::PatternScan(GameModule, "80 ?? B5 55 02 00 00");

    if (grainFilterOffset) {
        DWORD oldProtect;
        VirtualProtect((LPVOID)(grainFilterOffset), 6, PAGE_EXECUTE_READWRITE, &oldProtect);
        memset((void*)grainFilterOffset, 0x90, 6);
    }

    if (pauseMenuViewportResizeOffset) {
        DWORD oldProtect;
        VirtualProtect((LPVOID)(pauseMenuViewportResizeOffset), 7, PAGE_EXECUTE_READWRITE, &oldProtect);
        memset((void*)pauseMenuViewportResizeOffset, 0x90, 7);
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

