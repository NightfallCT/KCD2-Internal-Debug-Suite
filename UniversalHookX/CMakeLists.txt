cmake_minimum_required(VERSION 3.20)
project(UniversalHookX CXX C)

if(NOT (WIN32 AND MINGW))
    message(FATAL_ERROR
        "This project builds a Windows DLL with MinGW-w64. Configure with:\n"
        "  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.toolchain.cmake")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_C_STANDARD 11)

option(UHX_ENABLE_VULKAN "Build the Vulkan rendering backend" ON)

set(IMGUI_DIR   ${CMAKE_SOURCE_DIR}/src/dependencies/imgui)
set(MINHOOK_DIR ${CMAKE_SOURCE_DIR}/src/dependencies/minhook)

# ---- MinHook (C) ----------------------------------------------------------
add_library(minhook STATIC
    ${MINHOOK_DIR}/buffer.c
    ${MINHOOK_DIR}/hook.c
    ${MINHOOK_DIR}/trampoline.c
    ${MINHOOK_DIR}/hde/hde32.c
    ${MINHOOK_DIR}/hde/hde64.c
)
target_include_directories(minhook PUBLIC ${MINHOOK_DIR})

# ---- Dear ImGui -------------------------------------------------------
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_win32.cpp
    ${IMGUI_DIR}/imgui_impl_dx9.cpp
    ${IMGUI_DIR}/imgui_impl_dx10.cpp
    ${IMGUI_DIR}/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/imgui_impl_dx12.cpp
    ${IMGUI_DIR}/imgui_impl_opengl3.cpp
)

# ---- UniversalHookX ---------------------------------------------------
set(UHX_SOURCES
    src/dllmain.cpp
    src/console/console.cpp
    src/esp/esp.cpp
    src/fly/fly.cpp
    src/hooks/hooks.cpp
    src/hooks/ig-hooks.cpp
    src/hooks/backend/dx9/hook_directx9.cpp
    src/hooks/backend/dx10/hook_directx10.cpp
    src/hooks/backend/dx11/hook_directx11.cpp
    src/hooks/backend/dx12/hook_directx12.cpp
    src/hooks/backend/opengl/hook_opengl.cpp
    src/hooks/backend/vulkan/hook_vulkan.cpp
    src/menu/menu.cpp
    src/utils/utils.cpp
)

# hook_vulkan.cpp itself is always built: ENABLE_BACKEND_VULKAN in
# src/backend.hpp is commented out by default, so it compiles down to a
# harmless stub (same convention the other backends use) without needing
# the Vulkan SDK at all. Only go looking for the real SDK when this option
# is on, for the case where someone also flips on ENABLE_BACKEND_VULKAN.
if(UHX_ENABLE_VULKAN)
    find_path(VULKAN_INCLUDE_DIR vulkan/vulkan.h)
    find_library(VULKAN_LIBRARY NAMES vulkan-1)
    if(NOT VULKAN_INCLUDE_DIR OR NOT VULKAN_LIBRARY)
        message(FATAL_ERROR
            "UHX_ENABLE_VULKAN is ON but the Vulkan headers/import lib were not found "
            "for the MinGW target. Install Vulkan-Headers + a vulkan-1 import lib for "
            "x86_64-w64-mingw32, or reconfigure with -DUHX_ENABLE_VULKAN=OFF.")
    endif()
endif()

add_library(UniversalHookX SHARED ${UHX_SOURCES} ${IMGUI_SOURCES})

target_include_directories(UniversalHookX PRIVATE
    src
    ${IMGUI_DIR}
    ${MINHOOK_DIR}
    # Resolves "#include <Windows.h>" on case-sensitive filesystems; see
    # cmake/win32-case-shim/Windows.h.
    ${CMAKE_SOURCE_DIR}/cmake/win32-case-shim
)

target_compile_definitions(UniversalHookX PRIVATE
    ImTextureID=ImU64
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# MSVC implicitly converts function pointers to LPVOID at every
# MH_CreateHook() call site (non-standard, but accepted by MSVC/ICC). GCC
# rejects that conversion unless told to be permissive about it.
target_compile_options(UniversalHookX PRIVATE -fpermissive)

# The old MSVC/ICC build had whole-program/interprocedural optimization on;
# match that here so cross-TU calls in the hot entity-update path actually
# get inlined instead of paying a real call through every hook.
target_compile_options(UniversalHookX PRIVATE $<$<NOT:$<CONFIG:Debug>>:-flto>)
target_link_options(UniversalHookX PRIVATE $<$<NOT:$<CONFIG:Debug>>:-flto>)

target_link_libraries(UniversalHookX PRIVATE
    minhook
    d3d9 d3d10 d3d11 d3d12 dxgi d3dcompiler
    opengl32 gdi32 dwmapi user32 imm32
)

if(UHX_ENABLE_VULKAN)
    target_include_directories(UniversalHookX PRIVATE ${VULKAN_INCLUDE_DIR})
    target_link_libraries(UniversalHookX PRIVATE ${VULKAN_LIBRARY})
endif()

# Static runtime: this DLL gets injected into a third-party process, which
# won't have libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll on
# its search path.
target_link_options(UniversalHookX PRIVATE -static -static-libgcc -static-libstdc++)

# Forwards the FMODGetDSPDescription export to the renamed original plugin
# DLL so the game's FMOD loader keeps working while this DLL masquerades as
# the plugin. MSVC honors the #pragma comment(linker, "/export:...") in
# dllmain.cpp for this; GNU ld ignores that pragma entirely, so MinGW builds
# need the same forwarder declared via a .def file instead. Without this,
# ld also falls back to exporting every global symbol in the binary.
target_link_options(UniversalHookX PRIVATE ${CMAKE_SOURCE_DIR}/src/exports.def)

set_target_properties(UniversalHookX PROPERTIES
    PREFIX ""
    OUTPUT_NAME "KCD2-Internal $<CONFIG> x64"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
