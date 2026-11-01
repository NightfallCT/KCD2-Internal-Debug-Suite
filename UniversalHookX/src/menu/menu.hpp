#pragma once

#include <Windows.h>
#include "../dependencies/imgui/imgui.h"

namespace Menu {
    void InitializeContext(HWND hwnd);
    void Render( );

    inline bool bShowMenu = true;

    extern ImDrawList* drawlist;
} // namespace Menu
