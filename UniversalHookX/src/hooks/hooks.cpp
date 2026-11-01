#include <cstdio>
#include <mutex>
#include <thread>

#include "hooks.hpp"

#include "backend/dx10/hook_directx10.hpp"
#include "backend/dx11/hook_directx11.hpp"
#include "backend/dx12/hook_directx12.hpp"
#include "backend/dx9/hook_directx9.hpp"

#include "backend/opengl/hook_opengl.hpp"
#include "backend/vulkan/hook_vulkan.hpp"

#include "../console/console.hpp"
#include "../menu/menu.hpp"
#include "../utils/utils.hpp"

#include "../dependencies/minhook/MinHook.h"
#include "../hooks/ig-hooks.h"

static HWND g_hWindow = NULL;
static std::mutex g_mReinitHooksGuard;
using GetAsyncKeyStateFn = SHORT(WINAPI*)(int);
using GetKeyStateFn = SHORT(WINAPI*)(int);
using GetKeyboardStateFn = BOOL(WINAPI*)(PBYTE);
using GetRawInputDataFn = UINT(WINAPI*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
using GetRawInputBufferFn = UINT(WINAPI*)(PRAWINPUT, PUINT, UINT);
static GetAsyncKeyStateFn oGetAsyncKeyState = nullptr;
static GetKeyStateFn oGetKeyState = nullptr;
static GetKeyboardStateFn oGetKeyboardState = nullptr;
static GetRawInputDataFn oGetRawInputData = nullptr;
static GetRawInputBufferFn oGetRawInputBuffer = nullptr;

static bool ShouldBlockKeyboardStateKey(const int vKey) {
    // Keep internal control hotkeys responsive while menu is open.
    return vKey != VK_INSERT && vKey != VK_HOME && vKey != VK_END;
}

static SHORT WINAPI hkGetAsyncKeyState(int vKey) {
    if (Menu::bShowMenu && ShouldBlockKeyboardStateKey(vKey))
        return 0;
    return oGetAsyncKeyState ? oGetAsyncKeyState(vKey) : 0;
}

static SHORT WINAPI hkGetKeyState(int vKey) {
    if (Menu::bShowMenu && ShouldBlockKeyboardStateKey(vKey))
        return 0;
    return oGetKeyState ? oGetKeyState(vKey) : 0;
}

static BOOL WINAPI hkGetKeyboardState(PBYTE lpKeyState) {
    const BOOL result = oGetKeyboardState ? oGetKeyboardState(lpKeyState) : FALSE;
    if (!Menu::bShowMenu || lpKeyState == nullptr || !result)
        return result;

    // Block game keyboard polling while preserving internal control hotkeys.
    for (int vk = 0; vk < 256; ++vk) {
        if (ShouldBlockKeyboardStateKey(vk))
            lpKeyState[vk] = 0;
    }

    return TRUE;
}

static UINT WINAPI hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader) {
    if (Menu::bShowMenu && uiCommand == RID_INPUT) {
        // Prevent keyboard raw input packets from reaching game input handlers.
        if (pcbSize != nullptr)
            *pcbSize = 0;
        return 0;
    }
    return oGetRawInputData ? oGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader) : static_cast<UINT>(-1);
}

static UINT WINAPI hkGetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader) {
    if (Menu::bShowMenu) {
        if (pcbSize != nullptr)
            *pcbSize = 0;
        return 0;
    }
    return oGetRawInputBuffer ? oGetRawInputBuffer(pData, pcbSize, cbSizeHeader) : static_cast<UINT>(-1);
}

static DWORD WINAPI ReinitializeGraphicalHooks(LPVOID lpParam) {
    std::lock_guard<std::mutex> guard{g_mReinitHooksGuard};

    LOG("[!] Hooks will reinitialize!\n");

    HWND hNewWindow = U::GetProcessWindow( );
    while (hNewWindow == reinterpret_cast<HWND>(lpParam)) {
        hNewWindow = U::GetProcessWindow( );
    }

    H::bShuttingDown = true;

    H::Free( );
    H::Init( );

    H::bShuttingDown = false;
    Menu::bShowMenu = true;

    return 0;
}

static WNDPROC oWndProc;
static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto IsInputMessage = [](const UINT msg) -> bool {
        switch (msg) {
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
            case WM_SYSCHAR:
            case WM_INPUT:
                return true;
            default:
                return false;
        }
    };

    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_INSERT) {
            Menu::bShowMenu = !Menu::bShowMenu;
            return 0;
        } // else if (wParam == VK_HOME) {
        //     HANDLE hHandle = CreateThread(NULL, 0, ReinitializeGraphicalHooks, NULL, 0, NULL);
        //     if (hHandle != NULL)
        //         CloseHandle(hHandle);
        //     return 0;
        // }
        #ifdef _DEBUG
            if (wParam == VK_END) {

                igh::disableHooks( );
                H::bShuttingDown = true;
                U::UnloadDLL( );
                return 0;
            }
        #endif

    } else if (uMsg == WM_DESTROY) {
        HANDLE hHandle = CreateThread(NULL, 0, ReinitializeGraphicalHooks, hWnd, 0, NULL);
        if (hHandle != NULL)
            CloseHandle(hHandle);
    }

    LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    if (Menu::bShowMenu) {
        const LRESULT imguiResult = ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        if (imguiResult != 0 || IsInputMessage(uMsg))
            return 1;

        // (Doesn't work for some games like 'Sid Meier's Civilization VI')
        // Window may not maximize from taskbar because 'H::bShowDemoWindow' is set to true by default. ('hooks.hpp')
        //
        // return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) == 0;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

namespace Hooks {
    void Init( ) {
        g_hWindow = U::GetProcessWindow( );

#ifdef DISABLE_LOGGING_CONSOLE
        bool bNoConsole = GetConsoleWindow( ) == NULL;
        if (bNoConsole) {
            AllocConsole( );
        }
#endif

        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend( );
        switch (eRenderingBackend) {
            case DIRECTX9:
                DX9::Hook(g_hWindow);
                break;
            case DIRECTX10:
                DX10::Hook(g_hWindow);
                break;
            case DIRECTX11:
                DX11::Hook(g_hWindow);
                break;
            case DIRECTX12:
                DX12::Hook(g_hWindow);
                break;
            case OPENGL:
                GL::Hook(g_hWindow);
                break;
            case VULKAN:
                VK::Hook(g_hWindow);
                break;
        }

#ifdef DISABLE_LOGGING_CONSOLE
        if (bNoConsole) {
            FreeConsole( );
        }
#endif

        oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));

        MH_CreateHookApi(L"user32", "GetAsyncKeyState", &hkGetAsyncKeyState, reinterpret_cast<void**>(&oGetAsyncKeyState));
        MH_EnableHook(&GetAsyncKeyState);
        MH_CreateHookApi(L"user32", "GetKeyState", &hkGetKeyState, reinterpret_cast<void**>(&oGetKeyState));
        MH_EnableHook(&GetKeyState);
        MH_CreateHookApi(L"user32", "GetKeyboardState", &hkGetKeyboardState, reinterpret_cast<void**>(&oGetKeyboardState));
        MH_EnableHook(&GetKeyboardState);
        MH_CreateHookApi(L"user32", "GetRawInputData", &hkGetRawInputData, reinterpret_cast<void**>(&oGetRawInputData));
        MH_EnableHook(&GetRawInputData);
        MH_CreateHookApi(L"user32", "GetRawInputBuffer", &hkGetRawInputBuffer, reinterpret_cast<void**>(&oGetRawInputBuffer));
        MH_EnableHook(&GetRawInputBuffer);
    }

    void Free( ) {
        if (oWndProc) {
            SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        }

        MH_DisableHook(MH_ALL_HOOKS);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend( );
        switch (eRenderingBackend) {
            case DIRECTX9:
                DX9::Unhook( );
                break;
            case DIRECTX10:
                DX10::Unhook( );
                break;
            case DIRECTX11:
                DX11::Unhook( );
                break;
            case DIRECTX12:
                DX12::Unhook( );
                break;
            case OPENGL:
                GL::Unhook( );
                break;
            case VULKAN:
                VK::Unhook( );
                break;
        }
    }
} // namespace Hooks
