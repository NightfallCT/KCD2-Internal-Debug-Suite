#include "ig-hooks.h"
#include "hooks.hpp"
#include "../utils/seh_shim.hpp"

namespace IngameHooks {

    AddItemToInventory addItemFunc;
    DWORD64 moduleBase;
    DWORD64 addItemFuncAddr;

    DWORD64 disableMMAddr;

    DWORD64 playerHookAddr;
    DWORD64 playerHookRetAddr asm("playerHookRetAddr");
    C_Soul* localPlayer asm("localPlayer");

    DWORD64 CScriptSystemHookAddr;
    DWORD64 CScriptSystemHookRetAddr asm("CScriptSystemHookRetAddr");
    DWORD64* CScriptSystem asm("CScriptSystem");

    DWORD64 entityHookAddr;
    DWORD64 entityHookRetAddr asm("entityHookRetAddr");
    C_Soul* entityBuffer asm("entityBuffer");
    std::vector<C_Soul*> entityList;

    DWORD64 inventoryComponent asm("inventoryComponent");
    DWORD64 inventoryCompAddr;
    DWORD64 inventoryCompRetAddr asm("inventoryCompRetAddr");

    DWORD64 itemCollection asm("itemCollection");
    DWORD64 itemCollectionAddr;
    DWORD64 itemCollectionRetAddr asm("itemCollectionRetAddr");

    Viewmatrix* viewmatrix;

    bool tryAddEntity( );

    __declspec(naked) void hkEntities( ) {
        asm volatile (
            "pop %rax\n"

            "mov %rdx, entityBuffer(%rip)\n"
            "mov (%rdx), %rax\n"
            "mov %rsi, %rcx\n"
            "mov $0x18, %edx\n"
            "movzbl %r9b, %ebx\n"
            "jmp *entityHookRetAddr(%rip)\n"
        );
    }

    __attribute__((naked)) void hkLocalplayer( ) {
        asm volatile (
            "pop %rax\n"

            "mov %rdx, localPlayer(%rip)\n"

            "mov (%r8), %eax\n"
            "mov %r8, %rdi\n"
            "movl $0xBF800000, 0x38(%rsp)\n"

            "jmp *playerHookRetAddr(%rip)\n"
        );
    }

    __declspec(naked) void hkInvComp( ) {
        asm volatile (
            "pop %rax\n"

            "mov %r14, inventoryComponent(%rip)\n"

            "mov %rax, (%rbx)\n"
            "mov 0x8(%r14), %rdi\n"
            "mov 0x10(%r14), %rbp\n"
            "cmp %rbp, %rdi\n"

            "jmp *inventoryCompRetAddr(%rip)\n"
        );
    }

    __declspec(naked) void hkItemCol( ) {
        asm volatile (
            "pop %rax\n"

            "mov 0x20(%rax), %rcx\n"
            "mov 0x7E8(%rcx), %rcx\n"

            "push %rax\n"
            "mov 0x48(%rcx), %rax\n"
            "mov %r14, itemCollection(%rip)\n"
            "pop %rax\n"

            "mov (%rcx), %rax\n"

            "jmp *itemCollectionRetAddr(%rip)\n"
        );
    }

    __declspec(naked) void hkCScriptSystem( ) {
        asm volatile (
            "pop %rax\n"

            "mov %rcx, CScriptSystem(%rip)\n"

            "mov (%rcx), %rax\n"
            "mov %rcx, %rdi\n"
            "mov (%rdx), %rdx\n"
            "lea 0x20(%rsp), %rcx\n"

            "jmp *CScriptSystemHookRetAddr(%rip)\n"
        );
    }

    bool initHooks( ) {
        addItemFuncAddr = FindPatternIDA("WHGame.dll", "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 41 8A F9 48");
        if (addItemFuncAddr != NULL) {} 
        else {
            return false;
        }

        disableMMAddr = FindPatternIDA("WHGame.dll", "F2 0F 11 87 A0 00 00 00 8B 43 08 89");
        if (disableMMAddr != NULL) {} 
        else {
            return false;
        }

        playerHookAddr = FindPatternIDA("WHGame.DLL", "41 8B 00 49 8B F8 C7 44 24 38 00 00 80 BF");
        if (playerHookAddr != NULL) {
            playerHookRetAddr = playerHookAddr + 14;
            Hook((void*)playerHookAddr, (void*)hkLocalplayer, 14);
        } 
        else {
            return false;
        }

        CScriptSystemHookAddr = FindPatternIDA("WHGame.DLL", "48 8B 01 48 8B F9 48 8B 12 48 8D 4C 24 20 48 8B 98");
        if (CScriptSystemHookAddr != NULL) {
            CScriptSystemHookRetAddr = CScriptSystemHookAddr + 14;
            Hook((void*)CScriptSystemHookAddr, (void*)hkCScriptSystem, 14);
        } 
        else {
            return false;
        }

        entityHookAddr = FindPatternIDA("WHGame.DLL", "48 8B 02 48 8B CE BA 18 00 00 00 41 0F B6 D9");
        if (entityHookAddr != NULL) {
            entityHookRetAddr = entityHookAddr + 15;
            Hook((void*)entityHookAddr, (void*)hkEntities, 15);
        } 
        else {
            return false;
        }

        inventoryCompAddr = FindPatternIDA("WHGame.DLL", "48 89 03 49 8B 7E 08 49 8B 6E 10 48 3B FD");
        if (inventoryCompAddr != NULL) {
            inventoryCompRetAddr = inventoryCompAddr + 14;
            Hook((void*)inventoryCompAddr, (void*)hkInvComp, 14);
        } 
        else {
            return false;
        }

        itemCollectionAddr = FindPatternIDA("WHGame.DLL", "48 8B 48 20 48 8B 89 E8 07 00 00 48 8B 01 FF 50 28 F3 0F 10 35");
        if (itemCollectionAddr != NULL) {
            itemCollectionRetAddr = itemCollectionAddr + 14;
            Hook((void*)itemCollectionAddr, (void*)hkItemCol, 14);
        } 
        else {
            return false;
        }

        addItemFunc = reinterpret_cast<AddItemToInventory>(addItemFuncAddr);

        moduleBase = (DWORD64)GetModuleHandleA("WHGame.DLL");
        viewmatrix = reinterpret_cast<Viewmatrix*>(igh::moduleBase + 0x52B7FD8);

        return true;
    }

    void disableHooks( ) {
        if (playerHookAddr != NULL) {
            BYTE orig1[] = {0x41, 0x8B, 0x00, 0x49, 0x8B, 0xF8, 0xC7, 0x44, 0x24, 0x38, 0x00, 0x00, 0x80, 0xBF};
            writeBytes(playerHookAddr, orig1, 14);

            BYTE orig2[] = {0x48, 0x89, 0x03, 0x49, 0x8B, 0x7E, 0x08, 0x49, 0x8B, 0x6E, 0x10, 0x48, 0x3B, 0xFD};
            writeBytes(inventoryCompAddr, orig2, 14);

            BYTE orig3[] = {0x48, 0x8B, 0x48, 0x20, 0x48, 0x8B, 0x89, 0xE8, 0x07, 0x00, 0x00, 0x48, 0x8B, 0x01};
            writeBytes(itemCollectionAddr, orig3, 14);

            BYTE orig4[] = {0x48, 0x8B, 0x02, 0x48, 0x8B, 0xCE, 0xBA, 0x18, 0x00, 0x00, 0x00, 0x41, 0x0F, 0xB6, 0xD9};
            writeBytes(entityHookAddr, orig4, 15);

            BYTE orig5[] = {0xF2, 0x0F, 0x11, 0x87, 0xA0, 0x00, 0x00, 0x00, 0x8B, 0x43, 0x08, 0x89, 0x87, 0xA8, 0x00, 0x00, 0x00};
            writeBytes(disableMMAddr, orig5, 8);

            BYTE orig6[] = {0x48, 0x8B, 0x01, 0x48, 0x8B, 0xF9, 0x48, 0x8B, 0x12, 0x48, 0x8D, 0x4C, 0x24, 0x20};
            writeBytes(CScriptSystemHookAddr, orig6, 14);
        }
    }

    void addEntityLoop( ) {
        while (true) {
            if (H::bShuttingDown)
                break;

            SEH_TRY {
                tryAddEntity( );
            } SEH_EXCEPT {}
            
            if (H::bShuttingDown)
                break;

            Sleep(200);
        }
    }

    bool tryAddEntity() {
        if (entityBuffer != NULL && entityBuffer != localPlayer) {
            bool alreadyExists = false;

            for (size_t i = 0; i < entityList.size( ); i++) {
                if (entityList[i] == NULL || entityList[i]->health == 0) {
                    entityList.erase(entityList.begin( ) + i);
                    i--;
                    continue;
                }
                if (entityBuffer == entityList[i]) {
                    alreadyExists = true;
                    break;
                }
            }
            if (!alreadyExists) {
                entityList.push_back(entityBuffer);
            }
        }
        return true;
    }
}
