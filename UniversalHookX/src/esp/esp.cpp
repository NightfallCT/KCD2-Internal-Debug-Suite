#include "esp.h"
#include <cmath>
#include "../MyMem.h"
#include "../menu/menu.hpp"
#include "../dependencies/imgui/imgui.h"
#include "../hooks/ig-hooks.h"
#include "../classes/classes.h"
#include "../utils/seh_shim.hpp"

namespace esp {

    float Matrix[16];
    int matrixIndex = 0;
    int xres;
    int yres;
    float espRange = 50.0f;

    float Distance3D(const vec3_t& a, const vec3_t& b) {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float dz = b.z - a.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    bool WorldToScreen(vec3_t* pos, vec2& screen, float matrix[16], int windowWidth, int windowHeight) {
        // Matrix-vector Product, multiplying world(eye) coordinates by projection matrix = clipCoords
        vec4 clipCoords;
        clipCoords.x = pos->x * matrix[0] + pos->y * matrix[1] + pos->z * matrix[2] + matrix[3];
        clipCoords.y = pos->x * matrix[4] + pos->y * matrix[5] + pos->z * matrix[6] + matrix[7];
        clipCoords.z = pos->x * matrix[8] + pos->y * matrix[9] + pos->z * matrix[10] + matrix[11];
        clipCoords.w = pos->x * matrix[12] + pos->y * matrix[13] + pos->z * matrix[14] + matrix[15];

        if (clipCoords.w < 0.1f)
            return false;

        // perspective division, dividing by clip.W = Normalized Device Coordinates
        vec3_t NDC;
        NDC.x = clipCoords.x / clipCoords.w;
        NDC.y = clipCoords.y / clipCoords.w;
        NDC.z = clipCoords.z / clipCoords.w;

        screen.x = (windowWidth / 2 * NDC.x) + (NDC.x + windowWidth / 2);
        screen.y = -(windowHeight / 2 * NDC.y) + (NDC.y + windowHeight / 2);
        return true;
    }

    bool TryDrawEntity(size_t i, const float Matrix[16], int xres, int yres) {
        SEH_TRY {
            vec2 vScreen;
            vec3_t entPos = igh::entityList[i]->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords;
            if (Distance3D(entPos, igh::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords) > esp::espRange)
                return true;
            if (WorldToScreen(&entPos, vScreen, const_cast<float*>(Matrix), xres, yres)) {
                char copiedName[33] = "Unknown";
                auto* nameClass = igh::entityList[i]->pNameClass;
                if (nameClass && !IsBadReadPtr(nameClass, sizeof(NameClass))) {
                    if (nameClass->entityName[0] != '\0') {
                        strncpy_s(copiedName, sizeof(copiedName), nameClass->entityName, 32);
                        copiedName[32] = '\0';
                    }
                }
                char copiedFaction[17] = "None";
                auto* factionClass = igh::entityList[i]->pFactionClass;
                if (factionClass && !IsBadReadPtr(factionClass, sizeof(FactionClass))) {
                    if (factionClass->entityFaction[0] != '\0') {
                        strncpy_s(copiedFaction, sizeof(copiedFaction), factionClass->entityFaction, 16);
                        copiedFaction[16] = '\0';
                    }
                }
                char display[256];
                snprintf(display, sizeof(display), "Name: %s\nFaction: %s\nHealth: %.1f\nStamina: %.1f\nEnergy: %.1f",
                         copiedName, copiedFaction,
                         igh::entityList[i]->health,
                         igh::entityList[i]->stamina,
                         igh::entityList[i]->energy);
                Menu::drawlist->AddText(ImVec2(vScreen.x, vScreen.y - 10.0f), IM_COL32_WHITE, display);
            }
            return true;
        } SEH_EXCEPT {
            return false;
        }
    }

    void DrawNames( ) {
        Menu::drawlist->AddText(ImVec2(10.0f, 20.0f), IM_COL32_WHITE, "Drawing Names");
        SEH_TRY {
            xres = *(int*)(igh::moduleBase + 0x531BF8C);
            yres = *(int*)(igh::moduleBase + 0x531BF90);
            memcpy(&Matrix, (void*)hk::viewmatrix, sizeof(Matrix));
        } SEH_EXCEPT {
            return;
        }

        for (size_t i = 0; i < igh::entityList.size( ); i++) {
            TryDrawEntity(i, Matrix, xres, yres); // just ignore failures silently
        }
    }
}
