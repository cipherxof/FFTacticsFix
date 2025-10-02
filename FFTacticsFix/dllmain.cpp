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
    uintptr_t pauseMenuViewportResizeOffset = (uintptr_t)Memory::PatternScan(GameModule, "66 C7 80 ?? ?? ?? ?? 01 00");

    if (grainFilterOffset) {
        DWORD oldProtect;
        VirtualProtect((LPVOID)(grainFilterOffset), 6, PAGE_EXECUTE_READWRITE, &oldProtect);
        *(uint32_t*)(grainFilterOffset + 0) = 0x90909090;
        *(uint16_t*)(grainFilterOffset + 4) = 0x9090;
    }

    if (pauseMenuViewportResizeOffset) {
        DWORD oldProtect;
        VirtualProtect((LPVOID)(pauseMenuViewportResizeOffset), 9, PAGE_EXECUTE_READWRITE, &oldProtect);
        *(uint32_t*)(pauseMenuViewportResizeOffset + 0) = 0x90909090;
        *(uint32_t*)(pauseMenuViewportResizeOffset + 4) = 0x90909090;
        *(uint16_t*)(pauseMenuViewportResizeOffset + 8) = 0x90;
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

