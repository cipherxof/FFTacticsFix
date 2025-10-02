#include <shlwapi.h>
#include "Memory.h"
#include "MinHook.h"
#include <winternl.h>
#include <intrin.h>

HMODULE GameModule = GetModuleHandleA("FFT_enhanced.exe");
uintptr_t GameBase = (uintptr_t)GameModule;

void InstallHooks()
{
    //MH_Initialize();
}

void ApplyPatches()
{
    uintptr_t grainFilterOffset = (uintptr_t)Memory::PatternScan(GameModule, "38 ?? A0 55 02 00");

    if (!grainFilterOffset) {
        return;
    }

    DWORD oldProtect;
    VirtualProtect((LPVOID)(grainFilterOffset), 6, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(uint32_t*)(grainFilterOffset + 0) = 0x90909090;
    *(uint16_t*)(grainFilterOffset + 4) = 0x9090;
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
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

