#include <windows.h>
#include <iostream>
#include <thread>
#include <pmmintrin.h>

#include "console/console.hpp"

#include "hooks/hooks.hpp"
#include "utils/utils.hpp"
#include "utils/seh_shim.hpp"

#include "dependencies/minhook/MinHook.h"
#include "hooks/ig-hooks.h"
#include "MyMem.h"

#pragma comment(linker, "/export:FMODGetDSPDescription=Quatmosphere_orig.dll.FMODGetDSPDescription,@1")

#pragma comment(lib, "user32.lib")

DWORD WINAPI OnProcessAttach(LPVOID lpParam);
DWORD WINAPI OnProcessDetach(LPVOID lpParam);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);

        // MXCSR is per-thread CPU state, not per-module: whichever code sets
        // FTZ/DAZ affects float math for every module on this thread,
        // including the game engine's own. Set it explicitly so the game's
        // float-heavy code can't fall onto the slow denormal microcode path.
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

        SehShim::Install( );

        U::SetRenderingBackend(DIRECTX12);

        HANDLE hHandle = CreateThread(NULL, 0, OnProcessAttach, hinstDLL, 0, NULL);
        if (hHandle != NULL) {
            CloseHandle(hHandle);
        }
        // Create entity update thread
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)igh::addEntityLoop, hinstDLL, 0, NULL);

    } else if (fdwReason == DLL_PROCESS_DETACH && !lpReserved) {
        OnProcessDetach(NULL);
    }

    return TRUE;
}

DWORD WINAPI OnProcessAttach(LPVOID lpParam) {
    #ifndef _DEBUG
        Sleep(10000);
    #endif

    Console::Alloc( );
    LOG("[+] Rendering backend: %s\n", U::RenderingBackendToStr( ));
    if (U::GetRenderingBackend( ) == NONE) {
        LOG("[!] Looks like you forgot to set a backend. Will unload after pressing enter...");
        std::cin.get( );

        FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(lpParam), 0);
        return 0;
    }

    MH_Initialize( );
    H::Init( );

    if (!igh::initHooks( )) {
        MessageBoxA(0, "Hooking error occured!", "Warning", 0);
    }

    Console::Free( );

    return 0;
}

DWORD WINAPI OnProcessDetach(LPVOID lpParam) {
    H::Free( );
    MH_Uninitialize( );

    Console::Free( );

    return 0;
}
