#pragma once
#include "Windows.h"
#include "../MyMem.h"
#include "../classes/classes.h"
#include <iostream>


namespace IngameHooks {
    typedef DWORD64(__fastcall* AddItemToInventory)(DWORD64* invObj, struct ItemObject* itemObj, int itemType, char skipNote);

    extern AddItemToInventory addItemFunc;

    // asm() labels below pin the linker symbol name to the plain identifier
    // (no C++ mangling) so the naked-function inline asm in ig-hooks.cpp can
    // reference these globals by name directly.
    extern DWORD64 inventoryComponent asm("inventoryComponent");
    extern DWORD64 itemCollection asm("itemCollection");
    extern DWORD64 moduleBase;
    extern C_Soul* localPlayer asm("localPlayer");
    extern C_Soul* entityBuffer asm("entityBuffer");
    extern std::vector<C_Soul*> entityList;
    extern Viewmatrix* viewmatrix;
    extern DWORD64 disableMMAddr;
    extern DWORD64* CScriptSystem asm("CScriptSystem");
    extern bool initHooks( );
    extern void disableHooks( );
    extern void addEntityLoop( );
}

namespace igh = IngameHooks;
namespace hk = IngameHooks;
