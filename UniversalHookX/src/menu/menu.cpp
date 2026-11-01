#include "menu.hpp"

#include "../dependencies/imgui/imgui_impl_win32.h"
#include "../esp/esp.h"
#include "../fly/fly.h"
#include "../hooks/ig-hooks.h"
#include "../utils/seh_shim.hpp"
#include "item_map.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <string>
#include <vector>

namespace ig = ImGui;

namespace Menu {
    ImDrawList* drawlist;

    bool bInfHealth = false;
    bool bInfStamina = false;
    bool bInfEnergy = false;
    bool bInfFullness = false;

    // ══════════════════════════════════════════════════════════════════════════════
    //  Menu style settings — persisted for the lifetime of the process.
    // ══════════════════════════════════════════════════════════════════════════════
    struct MenuSettings {
        // Colours
        ImVec4 accentColor = ImVec4(0.163f, 0.273f, 0.495f, 1.00f); // header / title bar
        ImVec4 accentHovered = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
        ImVec4 accentActive = ImVec4(0.30f, 0.50f, 0.90f, 1.00f);
        ImVec4 frameColor = ImVec4(0.12f, 0.12f, 0.14f, 1.00f); // input / child bg
        ImVec4 windowBg = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        ImVec4 textColor = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

        // Sizing
        float fontScale = 1.0f;      // 0.75 – 1.50
        float windowRounding = 6.0f; // 0 – 14
        float frameRounding = 4.0f;  // 0 – 10
        float itemSpacingY = 4.0f;   // 2 – 12
        float windowAlpha = 1.0f;    // 0.40 – 1.00  (window bg alpha)

        // Misc
        bool showFps = false;
    };

    static MenuSettings s_settings;
    static std::string s_settingsSaveStatus = "";

    // ── Save/Load Settings ────────────────────────────────────────────────────
    static void SaveSettings( ) {
        char path[MAX_PATH];
        if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
            std::string s(path);
            size_t binPos = s.find("\\Bin\\");
            if (binPos == std::string::npos)
                binPos = s.find("/Bin/");

            if (binPos != std::string::npos) {
                std::string setPath = s.substr(0, binPos) + "\\menu_settings.bin";
                FILE* f = fopen(setPath.c_str( ), "wb");
                if (f) {
                    fwrite(&s_settings, sizeof(MenuSettings), 1, f);
                    fclose(f);
                    s_settingsSaveStatus = "Settings saved successfully!";
                    return;
                }
            }
        }
        s_settingsSaveStatus = "Error: Could not save settings.";
    }

    static void LoadSettings( ) {
        char path[MAX_PATH];
        if (GetModuleFileNameA(NULL, path, MAX_PATH)) {
            std::string s(path);
            size_t binPos = s.find("\\Bin\\");
            if (binPos == std::string::npos)
                binPos = s.find("/Bin/");

            if (binPos != std::string::npos) {
                std::string setPath = s.substr(0, binPos) + "\\menu_settings.bin";
                FILE* f = fopen(setPath.c_str( ), "rb");
                if (f) {
                    fread(&s_settings, sizeof(MenuSettings), 1, f);
                    fclose(f);
                }
            }
        }
    }

    // Push all style overrides; call before ig::Begin, pop after ig::End.
    static int s_pushedVars = 0;
    static int s_pushedCols = 0;

    static void PushMenuStyle( ) {
        s_pushedVars = 0;
        s_pushedCols = 0;

        ImVec4 winBg = s_settings.windowBg;
        winBg.w = s_settings.windowAlpha;

        ig::PushStyleColor(ImGuiCol_WindowBg, winBg);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_TitleBg, s_settings.accentColor);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_TitleBgActive, s_settings.accentColor);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_TitleBgCollapsed, s_settings.accentActive);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_Button, s_settings.accentColor);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_ButtonHovered, s_settings.accentHovered);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_ButtonActive, s_settings.accentActive);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_Header, s_settings.accentColor);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_HeaderHovered, s_settings.accentHovered);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_HeaderActive, s_settings.accentActive);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_FrameBg, s_settings.frameColor);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_Text, s_settings.textColor);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_CheckMark, s_settings.accentHovered);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_SliderGrab, s_settings.accentColor);
        s_pushedCols++;
        ig::PushStyleColor(ImGuiCol_SliderGrabActive, s_settings.accentActive);
        s_pushedCols++;

        ig::PushStyleVar(ImGuiStyleVar_WindowRounding, s_settings.windowRounding);
        s_pushedVars++;
        ig::PushStyleVar(ImGuiStyleVar_FrameRounding, s_settings.frameRounding);
        s_pushedVars++;
        ig::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                         ImVec2(8.0f, s_settings.itemSpacingY));
        s_pushedVars++;

        ig::GetIO( ).FontGlobalScale = s_settings.fontScale;
    }

    static void PopMenuStyle( ) {
        ig::PopStyleColor(s_pushedCols);
        ig::PopStyleVar(s_pushedVars);
    }

    namespace {
        enum class Category : int {
            Tutorial = 0,
            Inventory = 1,
            Debug = 2,
            Entities = 3,
            Lua = 4,
            Settings = 5
        };

        // ══════════════════════════════════════════════════════════════════════
        //  SEH_TRY-safe primitives
        // ══════════════════════════════════════════════════════════════════════

        static void RenderDebugMenu_SEH( ) {
            SEH_TRY {
                ig::Text("Localplayer: ");
                ig::Text("Health: %f", igh::localPlayer->health);
                ig::SliderFloat("##health", &igh::localPlayer->health, 0, 100.0f, "%.1f");
                ig::SameLine( );
                ig::Checkbox("Inf##1", &bInfHealth);

                ig::Text("Stamina: %f", igh::localPlayer->stamina);
                ig::SliderFloat("##stamina", &igh::localPlayer->stamina, 0, 250.0f, "%.1f");
                ig::SameLine( );
                ig::Checkbox("Inf##2", &bInfStamina);

                ig::Text("Fullness: %f", igh::localPlayer->fullness);
                ig::SliderFloat("##fullness", &igh::localPlayer->fullness, 0, 100.0f, "%.1f");
                ig::SameLine( );
                ig::Checkbox("Inf##3", &bInfFullness);

                ig::Text("Energy: %f", igh::localPlayer->energy);
                ig::SliderFloat("##energy", &igh::localPlayer->energy, 0, 100.0f, "%.1f");
                ig::SameLine( );
                ig::Checkbox("Inf##4", &bInfEnergy);

                ig::Text("Velocity: %f", igh::localPlayer->velocity);
                ig::Spacing( );
                if (ig::Checkbox("Fly", &Fly::flyEnabled)) {
                    if (Fly::flyEnabled) {
                        BYTE patched[] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
                        writeBytes(hk::disableMMAddr, patched, 17);
                    } else {
                        BYTE unpatched[] = {0xF2, 0x0F, 0x11, 0x87, 0xA0, 0x00, 0x00, 0x00, 0x8B, 0x43, 0x08, 0x89, 0x87, 0xA8, 0x00, 0x00, 0x00};
                        writeBytes(hk::disableMMAddr, unpatched, 17);
                    }
                }
                ig::SliderFloat("Fly Speed", &Fly::flySpeed, 5.0f, 100.0f, "%.0f");
                ig::Separator( );
                ig::Spacing( );
                ig::Text("Resolution: %ix%i", esp::xres, esp::yres);
                ig::Checkbox("Draw NPC Debug Info (Testing)", &esp::bEspEnabled);
                ig::SliderFloat("Distance", &esp::espRange, 0.0f, 300.0f, "%.0f");
            } SEH_EXCEPT {
            }
        }

        static void WriteInfiniteValues_SEH( ) {
            SEH_TRY {
                if (bInfEnergy)
                    igh::localPlayer->energy = 100.0f;
                if (bInfHealth)
                    igh::localPlayer->health = 100.0f;
                if (bInfStamina)
                    igh::localPlayer->stamina = 250.0f;
                if (bInfFullness)
                    igh::localPlayer->fullness = 100.0f;
            } SEH_EXCEPT {
            }
        }

        static void DrawESP_SEH( ) {
            SEH_TRY {
                if (esp::bEspEnabled)
                    esp::DrawNames( );
            } SEH_EXCEPT {
            }
        }

        static bool ReadEntityName_SEH(C_Soul* pSoul, char* outName, int outSize) {
            SEH_TRY {
                if (!pSoul || !pSoul->pNameClass)
                    return false;
                memset(outName, 0, outSize);
                const int kFullSize = 0x20;
                const int copy = (outSize - 1 < kFullSize) ? outSize - 1 : kFullSize;
                memcpy(outName, pSoul->pNameClass, copy);
                return true;
            } SEH_EXCEPT {
                return false;
            }
        }

        static bool ReadEntityFaction_SEH(C_Soul* pSoul, char* outFaction, int outSize) {
            SEH_TRY {
                if (!pSoul || !pSoul->pFactionClass)
                    return false;
                memset(outFaction, 0, outSize);
                const int copy = (outSize - 1 < 32) ? outSize - 1 : 32;
                memcpy(outFaction, pSoul->pFactionClass->entityFaction, copy);
                return true;
            } SEH_EXCEPT {
                return false;
            }
        }

        static bool ReadLocalPlayerCoords_SEH(vec3_t* outCoords) {
            SEH_TRY {
                if (igh::localPlayer &&
                    igh::localPlayer->pC_Entity &&
                    igh::localPlayer->pC_Entity->pC_GameObject &&
                    igh::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy &&
                    igh::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity) {
                    *outCoords = igh::localPlayer->pC_Entity->pC_GameObject
                                     ->pC_PhysicalProxy->pC_LivingEntity->coords;
                    return true;
                }
            } SEH_EXCEPT {
            }
            return false;
        }

        static bool TeleportEntity_SEH(C_Soul* pSoul, const vec3_t* target) {
            SEH_TRY {
                if (pSoul &&
                    pSoul->pC_Entity &&
                    pSoul->pC_Entity->pC_GameObject &&
                    pSoul->pC_Entity->pC_GameObject->pC_PhysicalProxy &&
                    pSoul->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity) {
                    pSoul->pC_Entity->pC_GameObject
                        ->pC_PhysicalProxy->pC_LivingEntity->coords = *target;
                    return true;
                }
            } SEH_EXCEPT {
            }
            return false;
        }

        static bool ReadEntityCoords_SEH(C_Soul* pSoul, vec3_t* out) {
            SEH_TRY {
                if (pSoul &&
                    pSoul->pC_Entity &&
                    pSoul->pC_Entity->pC_GameObject &&
                    pSoul->pC_Entity->pC_GameObject->pC_PhysicalProxy &&
                    pSoul->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity) {
                    *out = pSoul->pC_Entity->pC_GameObject
                               ->pC_PhysicalProxy->pC_LivingEntity->coords;
                    return true;
                }
            } SEH_EXCEPT {
            }
            return false;
        }

        static bool TeleportLocalPlayer_SEH(const vec3_t* target) {
            SEH_TRY {
                if (igh::localPlayer &&
                    igh::localPlayer->pC_Entity &&
                    igh::localPlayer->pC_Entity->pC_GameObject &&
                    igh::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy &&
                    igh::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity) {
                    igh::localPlayer->pC_Entity->pC_GameObject
                        ->pC_PhysicalProxy->pC_LivingEntity->coords = *target;
                    return true;
                }
            } SEH_EXCEPT {
            }
            return false;
        }

        static float Distance3D(const vec3_t& a, const vec3_t& b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return sqrtf(dx * dx + dy * dy + dz * dz);
        }

        static bool SnapFreezeCoords_SEH(C_Soul* pSoul, vec3_t* outSnapshot) {
            SEH_TRY {
                if (pSoul &&
                    pSoul->pC_Entity &&
                    pSoul->pC_Entity->pC_GameObject &&
                    pSoul->pC_Entity->pC_GameObject->pC_PhysicalProxy &&
                    pSoul->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity) {
                    *outSnapshot = pSoul->pC_Entity->pC_GameObject
                                       ->pC_PhysicalProxy->pC_LivingEntity->coords;
                    return true;
                }
            } SEH_EXCEPT {
            }
            return false;
        }

        struct FrozenEntry {
            C_Soul* pSoul;
            vec3_t coords;
        };
        static FrozenEntry s_frozenEntities[256];
        static int s_frozenCount = 0;

        static void WriteFrozenEntities_SEH( ) {
            SEH_TRY {
                for (int i = 0; i < s_frozenCount; i++) {
                    C_Soul* p = s_frozenEntities[i].pSoul;
                    if (p &&
                        p->pC_Entity &&
                        p->pC_Entity->pC_GameObject &&
                        p->pC_Entity->pC_GameObject->pC_PhysicalProxy &&
                        p->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity) {
                        p->pC_Entity->pC_GameObject
                            ->pC_PhysicalProxy->pC_LivingEntity->coords =
                            s_frozenEntities[i].coords;
                    }
                }
            } SEH_EXCEPT {
            }
        }

        static bool IsEntityFrozen(C_Soul* pSoul) {
            for (int i = 0; i < s_frozenCount; i++)
                if (s_frozenEntities[i].pSoul == pSoul)
                    return true;
            return false;
        }

        static bool ToggleEntityFreeze(C_Soul* pSoul) {
            for (int i = 0; i < s_frozenCount; i++) {
                if (s_frozenEntities[i].pSoul == pSoul) {
                    s_frozenEntities[i] = s_frozenEntities[s_frozenCount - 1];
                    s_frozenCount--;
                    return false;
                }
            }
            if (s_frozenCount < 256) {
                vec3_t snap{ };
                if (SnapFreezeCoords_SEH(pSoul, &snap)) {
                    s_frozenEntities[s_frozenCount++] = {pSoul, snap};
                    return true;
                }
            }
            return false;
        }

        // ══════════════════════════════════════════════════════════════════════
        //  String / collection helpers  — NO SEH_TRY
        // ══════════════════════════════════════════════════════════════════════

        static std::string TrimCopy(const std::string& text) {
            std::size_t start = 0;
            while (start < text.size( ) && std::isspace(static_cast<unsigned char>(text[start])))
                ++start;
            std::size_t end = text.size( );
            while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])))
                --end;
            return text.substr(start, end - start);
        }

        static std::string ToLowerCopy(const std::string& text) {
            std::string s = text;
            std::transform(s.begin( ), s.end( ), s.begin( ), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        }

        static bool ContainsAny(const std::string& haystack,
                                std::initializer_list<const char*> needles) {
            for (const char* n : needles)
                if (haystack.find(n) != std::string::npos)
                    return true;
            return false;
        }

        // ── Inventory ─────────────────────────────────────────────────────────

        struct InventoryEntry {
            std::string category;
            std::string itemName;
            std::string itemNameLower;
            ItemDatabase::ItemId itemId;
        };

        static std::string ExtractCategory(const std::string& itemName) {
            const std::string key = ToLowerCopy(itemName);
            if (ContainsAny(key, {"bread", "meat", "cheese", "apple", "pear", "onion", "garlic", "egg",
                                  "mushroom", "cake", "soup", "stew", "fish", "sausage", "turnip", "carrot"}))
                return "Food";
            if (ContainsAny(key, {"potion", "elixir", "decoction", "remedy", "phial", "medicine"}))
                return "Potions";
            if (ContainsAny(key, {"alchemy", "herb", "flower", "mint", "sage", "wormwood", "nettle",
                                  "charcoal", "sulphur", "ore", "powder", "ingredient"}))
                return "Ingredients";
            if (ContainsAny(key, {"wine", "beer", "ale", "spirit", "alcohol", "water"}))
                return "Drinks";
            if (ContainsAny(key, {"sword", "axe", "mace", "warhammer", "dagger", "sabre", "halberd", "spear", "weapon"}))
                return "Weapons";
            if (ContainsAny(key, {"bow", "crossbow"}))
                return "Ranged Weapons";
            if (ContainsAny(key, {"arrow", "bolt", "quarrel"}))
                return "Ammo";
            if (ContainsAny(key, {"shield", "buckler", "pavise"}))
                return "Shields";
            if (ContainsAny(key, {"helmet", "kettlehat", "coif", "hood", "hat", "cowl", "head", "cap"}))
                return ContainsAny(key, {"plate", "mail", "chain", "brigandine", "armor", "armour"})
                           ? "Head Armor"
                           : "Head Clothing";
            if (ContainsAny(key, {"gauntlet", "glove", "vambrace", "arm"}))
                return ContainsAny(key, {"plate", "mail", "chain", "brigandine", "armor", "armour"})
                           ? "Hand/Arm Armor"
                           : "Hand/Arm Clothing";
            if (ContainsAny(key, {"boot", "shoe", "sabat", "foot"}))
                return ContainsAny(key, {"plate", "mail", "chain", "armor", "armour"})
                           ? "Foot Armor"
                           : "Foot Clothing";
            if (ContainsAny(key, {"leg", "hose", "trouser", "pants", "greave"}))
                return ContainsAny(key, {"plate", "mail", "chain", "armor", "armour"})
                           ? "Leg Armor"
                           : "Leg Clothing";
            if (ContainsAny(key, {"cuirass", "gambeson", "tunic", "surcoat", "doublet", "jacket",
                                  "wafenrock", "waffenrock", "brigandine", "mail", "plate", "chest", "Coat", "Habit"}))
                return ContainsAny(key, {"plate", "mail", "chain", "brigandine", "armor", "armour", "cuirass"})
                           ? "Body Armor"
                           : "Body Clothing";
            if (ContainsAny(key, {"ring", "necklace", "brooch", "gem", "jewel"}))
                return "Jewelry";
            if (ContainsAny(key, {"book", "letter", "map", "scroll", "document", "diary", "orders"}))
                return "Documents";
            if (ContainsAny(key, {"key", "lockpick"}))
                return "Keys";
            if (ContainsAny(key, {"dice", "badge", "die"}))
                return "Dice";
            if (ContainsAny(key, {"coin", "groschen", "chalice", "cup", "goblet", "treasure"}))
                return "Valuables";
            if (ContainsAny(key, {"horse", "saddle", "spur", "bridle"}))
                return "Horse Gear";
            if (ContainsAny(key, {"tool", "kit", "hammer", "anvil", "needle", "thread", "rope"}))
                return "Tools";
            if (ContainsAny(key, {"quest", "alias_", "story", "cutscene"}))
                return "Quest Items";
            return "Other Items";
        }

        static void BuildInventoryView(std::vector<std::string>& outCategories,
                                       std::vector<InventoryEntry>& outEntries) {
            outCategories.clear( );
            outEntries.clear( );
            outEntries.reserve(ItemDatabase::kItemEntryCount);

            for (std::size_t i = 0; i < ItemDatabase::kItemEntryCount; i++) {
                const ItemDatabase::ItemEntry& e = ItemDatabase::kItemEntries[i];
                InventoryEntry inv;
                inv.itemName = e.name;
                inv.itemNameLower = ToLowerCopy(inv.itemName);
                inv.category = ExtractCategory(inv.itemName);
                inv.itemId = e.id;
                outEntries.push_back(inv);
                if (std::find(outCategories.begin( ), outCategories.end( ), inv.category) == outCategories.end( ))
                    outCategories.push_back(inv.category);
            }

            static const std::vector<std::string> kOrder = {
                "Weapons", "Ranged Weapons", "Ammo", "Shields",
                "Head Armor", "Body Armor", "Hand/Arm Armor", "Leg Armor", "Foot Armor",
                "Head Clothing", "Body Clothing", "Hand/Arm Clothing", "Leg Clothing", "Foot Clothing",
                "Food", "Drinks", "Ingredients", "Potions",
                "Tools", "Horse Gear", "Documents", "Keys", "Dice", "Jewelry", "Valuables", "Quest Items", "Other Items"};
            std::stable_sort(outCategories.begin( ), outCategories.end( ),
                             [&](const std::string& a, const std::string& b) {
                                 const auto ia = std::find(kOrder.begin( ), kOrder.end( ), a);
                                 const auto ib = std::find(kOrder.begin( ), kOrder.end( ), b);
                                 const bool ak = (ia != kOrder.end( )), bk = (ib != kOrder.end( ));
                                 if (ak && bk)
                                     return ia < ib;
                                 if (ak != bk)
                                     return ak;
                                 return a < b;
                             });
            std::sort(outEntries.begin( ), outEntries.end( ), [](const InventoryEntry& a, const InventoryEntry& b) {
                return (a.category != b.category) ? a.category < b.category : a.itemName < b.itemName;
            });
        }

        // ── Entities ──────────────────────────────────────────────────────────

        struct EntityEntry {
            std::string name;
            std::string nameLower;
            std::string faction;
            C_Soul* pSoul = nullptr;
            // Live distance from the local player in metres. -1 means "unknown"
            float distance = -1.0f;
        };

        static void BuildEntityView(std::vector<EntityEntry>& outEntries) {
            outEntries.clear( );
            if (igh::entityList.empty( ))
                return;

            vec3_t playerPos{ };
            const bool havePlayer = ReadLocalPlayerCoords_SEH(&playerPos);

            outEntries.reserve(igh::entityList.size( ));
            for (C_Soul* pSoul : igh::entityList) {
                if (!pSoul)
                    continue;
                char nameBuf[33] = { };
                char factionBuf[33] = { };
                ReadEntityName_SEH(pSoul, nameBuf, sizeof(nameBuf));
                ReadEntityFaction_SEH(pSoul, factionBuf, sizeof(factionBuf));

                EntityEntry e;
                e.pSoul = pSoul;
                e.name = TrimCopy(nameBuf);
                e.faction = TrimCopy(factionBuf);
                if (e.name.empty( ))
                    e.name = "Unknown";
                if (e.faction.empty( ))
                    e.faction = "Unknown";
                e.nameLower = ToLowerCopy(e.name);

                vec3_t ec{ };
                if (havePlayer && ReadEntityCoords_SEH(pSoul, &ec))
                    e.distance = Distance3D(playerPos, ec);
                else
                    e.distance = -1.0f;

                outEntries.push_back(e);
            }
            std::sort(outEntries.begin( ), outEntries.end( ), [](const EntityEntry& a, const EntityEntry& b) { return a.name < b.name; });
        }

        static void UpdateEntityDistances(std::vector<EntityEntry>& entries) {
            vec3_t playerPos{ };
            if (!ReadLocalPlayerCoords_SEH(&playerPos)) {
                for (auto& e : entries)
                    e.distance = -1.0f;
                return;
            }
            for (auto& e : entries) {
                vec3_t ec{ };
                if (ReadEntityCoords_SEH(e.pSoul, &ec))
                    e.distance = Distance3D(playerPos, ec);
                else
                    e.distance = -1.0f;
            }
        }

        // ── Panel renderers ───────────────────────────────────────────────────

        static void RenderTutorialPanel(const bool ingame) {
            ig::TextWrapped("A comprehensive toolkit for world and player manipulation.");
            ig::Spacing( );

            if (!ingame) {
                ig::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Status: DISCONNECTED");
                ig::TextWrapped("Most features require an active savegame to be loaded.");
            } else {
                ig::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Status: CONNECTED");
                ig::TextWrapped("Game hooks are active. All systems nominal.");
            }

            ig::Spacing( );
            ig::Separator( );
            ig::Spacing( );

            if (ig::CollapsingHeader("1. Inventory & Spawner")) {
                ig::BulletText("Load your save and open your in-game inventory ONCE.");
                ig::BulletText("This allows the cheat to find the 'Item Collection' pointers.");
                ig::BulletText("Search by name or category, then click 'Add Item'.");
                ig::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Warning: Spawning Quest Items can soft-lock your save.");
            }
            if (ig::CollapsingHeader("2. Entity Management")) {
                ig::BulletText("Press 'Refresh List' to scan the current area for NPCs.");
                ig::BulletText("Use 'Teleport to Me' to bring NPCs directly to your face.");
                ig::BulletText("The 'Freeze' toggle prevents NPCs from moving or attacking.");
            }
            if (ig::CollapsingHeader("3. Debug & Stat Tweaks")) {
                ig::BulletText("The Debug tab allows real-time editing of Player Health/Stamina.");
                ig::BulletText("Check 'Inf' next to a stat to lock it at its maximum value.");
                ig::BulletText("Toggle the NPC ESP to see debug information through walls.");
            }
            if (ig::CollapsingHeader("4. Keybinds & Safety")) {
                ig::Text("Controls:");
                ig::BulletText("[INSERT] - Toggle Menu Visibility.");
                ig::Spacing( );
                ig::Text("Safety Recommendations:");
                ig::BulletText("Always backup your save folder regularly.");
                ig::BulletText("If the menu crashes, the game will usually stay running (SEH-guarded).");
            }

            ig::Spacing( );
            ig::Separator( );
            ig::Spacing( );

            ig::TextDisabled("Developed by:");
            ig::SameLine( );
            ig::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "NightfallCT");
            if (ig::Button("Copy Discord Handle"))
                ig::SetClipboardText("NightfallCT");
        }

        static std::string FormatItemIdUuid(const ItemDatabase::ItemId& id) {
            char buf[37];
            snprintf(buf, sizeof(buf),
                     "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     id[3], id[2], id[1], id[0],
                     id[5], id[4],
                     id[7], id[6],
                     id[8], id[9],
                     id[10], id[11], id[12], id[13], id[14], id[15]);
            return std::string(buf);
        }

        static std::string ToPascalCase(const std::string& name) {
            std::string out;
            out.reserve(name.size( ));
            bool capNext = true;
            for (char c : name) {
                if (c == '_') {
                    capNext = true;
                    continue;
                }
                if (capNext) {
                    out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                    capNext = false;
                } else {
                    out.push_back(c);
                }
            }
            return out;
        }

        static void RenderInventoryPanel( ) {
            static bool initialized = false;
            static std::vector<std::string> categories;
            static std::vector<InventoryEntry> entries;
            static std::vector<int> filteredIndices;
            static int selectedCategoryIdx = 0;
            static int selectedItemIdx = 0;
            static int amount = 1;
            static std::string lastAction;
            static char itemSearchBuf[256] = "";

            if (!initialized) {
                BuildInventoryView(categories, entries);
                initialized = true;
            }

            if (categories.empty( )) {
                ig::TextDisabled("No items available in ItemDatabase");
                return;
            }

            selectedCategoryIdx = (std::max)(0, (std::min)(selectedCategoryIdx, (int)categories.size( ) - 1));
            const std::string& selCat = categories[selectedCategoryIdx];
            const std::string searchTrim = TrimCopy(itemSearchBuf);
            const std::string queryLower = searchTrim.empty( ) ? std::string( ) : ToLowerCopy(searchTrim);
            const bool searchActive = !queryLower.empty( );

            filteredIndices.clear( );
            filteredIndices.reserve(entries.size( ));
            for (int i = 0; i < (int)entries.size( ); i++) {
                if (searchActive) {
                    if (entries[i].itemNameLower.find(queryLower) != std::string::npos)
                        filteredIndices.push_back(i);
                } else if (entries[i].category == selCat) {
                    filteredIndices.push_back(i);
                }
            }

            if (!filteredIndices.empty( ))
                selectedItemIdx = (std::max)(0, (std::min)(selectedItemIdx, (int)filteredIndices.size( ) - 1));
            else
                selectedItemIdx = 0;

            if (!igh::inventoryComponent)
                ig::TextDisabled("Open your inventory to populate the necessary addresses");
            if (!igh::itemCollection)
                ig::TextDisabled("Waiting for Item Collection...");

            if (igh::inventoryComponent && igh::itemCollection) {
                if (searchActive)
                    ig::TextDisabled("Search active: showing matches from all categories.");
                else
                    ig::TextDisabled("Select a category and an item.");
                ig::TextDisabled("Loaded items: %d", (int)entries.size( ));
                ig::Spacing( );

                ig::SetNextItemWidth(-1.f);
                ig::InputTextWithHint("##item_search", "Search items by name...", itemSearchBuf, sizeof(itemSearchBuf));
                ig::Spacing( );

                if (ig::BeginTable("##inv_picker", 2, ImGuiTableFlags_SizingFixedFit)) {
                    ig::TableSetupColumn("Categories", ImGuiTableColumnFlags_WidthFixed, 220.f);
                    ig::TableSetupColumn("Items", ImGuiTableColumnFlags_WidthStretch);
                    ig::TableNextRow( );

                    ig::TableSetColumnIndex(0);
                    if (ig::BeginChild("##inv_categories", ImVec2(0, ig::GetContentRegionAvail( ).y - 80.f), true)) {
                        for (int i = 0; i < (int)categories.size( ); i++)
                            if (ig::Selectable(categories[i].c_str( ), selectedCategoryIdx == i)) {
                                selectedCategoryIdx = i;
                                selectedItemIdx = 0;
                            }
                    }
                    ig::EndChild( );

                    ig::TableSetColumnIndex(1);
                    if (ig::BeginChild("##inv_items", ImVec2(0, ig::GetContentRegionAvail( ).y - 80.f), true)) {
                        if (filteredIndices.empty( )) {
                            ig::TextDisabled(searchActive ? "No items match your search." : "No items in this category.");
                        } else {
                            for (int i = 0; i < (int)filteredIndices.size( ); i++) {
                                const InventoryEntry& e = entries[filteredIndices[i]];
                                if (ig::Selectable(e.itemName.c_str( ), selectedItemIdx == i))
                                    selectedItemIdx = i;
                            }
                        }
                    }
                    ig::EndChild( );
                    ig::EndTable( );
                }

                ig::Spacing( );
                ig::Separator( );
                ig::Spacing( );
                ig::SetNextItemWidth(160.f);
                ig::InputInt("Amount", &amount, 1, 10);
                if (amount < 1)
                    amount = 1;

                ig::SameLine( );
                const bool canAdd = !filteredIndices.empty( ) && igh::addItemFunc != NULL;
                if (!canAdd)
                    ig::BeginDisabled( );

                if (ig::Button("Add item", ImVec2(140.f, 0.f))) {
                    if (!igh::inventoryComponent || !igh::itemCollection) {
                        lastAction = "Open inventory first to populate pointers.";
                    } else if (amount <= 0) {
                        lastAction = "Item amount must be > 0.";
                    } else {
                        const InventoryEntry& sel = entries[filteredIndices[selectedItemIdx]];
                        if (sel.itemId[0] == 0x00) {
                            lastAction = "Item ID invalid (first byte is 0).";
                        } else {
                            ItemObject buf{ };
                            memcpy(buf.itemIDPart1, sel.itemId.data( ), sel.itemId.size( ));
                            buf.itemIDPart2 = 0x25;
                            buf.itemAmount = amount;
                            buf.durability = -1.0f;
                            buf.pItemCollection1 = &igh::itemCollection;
                            buf.pItemCollection2 = &igh::itemCollection;
                            buf.pItemCollection3 = &igh::itemCollection;
                            buf.pItemCollection4 = &igh::itemCollection;
                            const DWORD64 result = igh::addItemFunc(
                                reinterpret_cast<DWORD64*>(igh::inventoryComponent),
                                reinterpret_cast<ItemObject*>(&buf), 7, 0);
                            lastAction = (result != 0)
                                             ? ("Added: " + sel.itemName + " x" + std::to_string(amount))
                                             : "Add failed: game function returned 0.";
                        }
                    }
                }

                if (!canAdd)
                    ig::EndDisabled( );

                ig::SameLine( );
                const bool canCopy = !filteredIndices.empty( );
                if (!canCopy)
                    ig::BeginDisabled( );

                if (ig::Button("Copy Item ID", ImVec2(140.f, 0.f))) {
                    const InventoryEntry& sel = entries[filteredIndices[selectedItemIdx]];
                    const std::string formatted =
                        ToPascalCase(sel.itemName) + " Item ID: " + FormatItemIdUuid(sel.itemId);
                    ig::SetClipboardText(formatted.c_str( ));
                    lastAction = "Copied to clipboard: " + formatted;
                }

                if (!canCopy)
                    ig::EndDisabled( );

                if (!lastAction.empty( )) {
                    const bool fail = (lastAction.rfind("Add failed", 0) == 0 ||
                                       lastAction.rfind("Selected item", 0) == 0);
                    ig::Spacing( );
                    ig::TextColored(fail ? ImVec4(1.f, 0.35f, 0.35f, 1.f) : ImVec4(0.55f, 0.85f, 0.55f, 1.f),
                                    "%s", lastAction.c_str( ));
                }
            }
        }

        static void RenderEntityPanel( ) {
            static std::vector<EntityEntry> entityEntries;
            static std::vector<int> filteredIdx;
            static int selectedIdx = 0;
            static std::string lastAction;
            static char searchBuf[256] = "";
            static bool sortByDistance = false;

            if (ig::Button("Refresh List")) {
                entityEntries.clear( );
                BuildEntityView(entityEntries);
                selectedIdx = 0;
                lastAction = "List refreshed. Found " + std::to_string(entityEntries.size( )) + " entities.";
            }
            ig::SameLine( );
            ig::TextDisabled("(%d entities loaded)", (int)entityEntries.size( ));
            if (ig::Button("Clear List")) {
                igh::entityList.clear( );
                entityEntries.clear( );
            }
            ig::SameLine( );
            ig::Checkbox("Sort by distance", &sortByDistance);
            ig::Spacing( );

            if (entityEntries.empty( )) {
                ig::TextDisabled("No entities found. Make sure igh::entityList is populated,");
                ig::TextDisabled("then press 'Refresh List'.");
                return;
            }

            UpdateEntityDistances(entityEntries);

            ig::SetNextItemWidth(-1.f);
            ig::InputTextWithHint("##ent_search", "Search by name or faction...", searchBuf, sizeof(searchBuf));
            ig::Spacing( );

            const std::string queryLower = ToLowerCopy(TrimCopy(searchBuf));
            const bool searchActive = !queryLower.empty( );

            filteredIdx.clear( );
            filteredIdx.reserve(entityEntries.size( ));
            for (int i = 0; i < (int)entityEntries.size( ); i++) {
                if (!searchActive) {
                    filteredIdx.push_back(i);
                    continue;
                }
                if (entityEntries[i].nameLower.find(queryLower) != std::string::npos ||
                    ToLowerCopy(entityEntries[i].faction).find(queryLower) != std::string::npos)
                    filteredIdx.push_back(i);
            }

            if (sortByDistance) {
                std::sort(filteredIdx.begin( ), filteredIdx.end( ),
                          [&](int a, int b) {
                              const float da = entityEntries[a].distance;
                              const float db = entityEntries[b].distance;
                              if (da < 0.f && db < 0.f)
                                  return false;
                              if (da < 0.f)
                                  return false;
                              if (db < 0.f)
                                  return true;
                              return da < db;
                          });
            }

            if (!filteredIdx.empty( ))
                selectedIdx = (std::max)(0, (std::min)(selectedIdx, (int)filteredIdx.size( ) - 1));
            else
                selectedIdx = 0;

            const float footerReserve = 70.f;
            if (ig::BeginChild("##ent_list", ImVec2(0.f, ig::GetContentRegionAvail( ).y - footerReserve), true)) {
                if (filteredIdx.empty( )) {
                    ig::TextDisabled(searchActive ? "No entities match your search." : "No entities.");
                } else {
                    ig::TextDisabled("  %-28s  %-18s  %s", "Name", "Faction", "Dist");
                    ig::Separator( );
                    for (int i = 0; i < (int)filteredIdx.size( ); i++) {
                        const EntityEntry& e = entityEntries[filteredIdx[i]];
                        char distBuf[16];
                        if (e.distance >= 0.0f)
                            snprintf(distBuf, sizeof(distBuf), "%6.1fm", e.distance);
                        else
                            snprintf(distBuf, sizeof(distBuf), "     ?  ");

                        char label[192];
                        snprintf(label, sizeof(label), "%-28s  %-18s  %s##e%d",
                                 e.name.c_str( ), e.faction.c_str( ), distBuf, i);
                        if (ig::Selectable(label, selectedIdx == i, ImGuiSelectableFlags_SpanAllColumns))
                            selectedIdx = i;
                    }
                }
            }
            ig::EndChild( );

            ig::Spacing( );
            ig::Separator( );
            ig::Spacing( );

            const bool hasSelection = !filteredIdx.empty( );
            if (!hasSelection)
                ig::BeginDisabled( );

            if (ig::Button("Teleport to Me", ImVec2(140.f, 0.f))) {
                const EntityEntry& e = entityEntries[filteredIdx[selectedIdx]];
                vec3_t pos{ };
                if (ReadLocalPlayerCoords_SEH(&pos)) {
                    lastAction = TeleportEntity_SEH(e.pSoul, &pos)
                                     ? ("Teleported '" + e.name + "' to your location.")
                                     : ("Teleport failed: broken pointer chain for '" + e.name + "'.");
                } else {
                    lastAction = "Teleport failed: could not read local player position.";
                }
            }

            ig::SameLine( );
            if (ig::Button("Teleport to Entity", ImVec2(150.f, 0.f))) {
                const EntityEntry& e = entityEntries[filteredIdx[selectedIdx]];
                vec3_t target{ };
                if (ReadEntityCoords_SEH(e.pSoul, &target)) {
                    lastAction = TeleportLocalPlayer_SEH(&target)
                                     ? ("Teleported to '" + e.name + "'.")
                                     : ("Teleport failed: could not write player position.");
                } else {
                    lastAction = "Teleport failed: could not read entity position.";
                }
            }

            ig::SameLine( );
            const bool frozen = hasSelection && IsEntityFrozen(entityEntries[filteredIdx[selectedIdx]].pSoul);
            if (ig::Button(frozen ? "Unfreeze" : "Freeze", ImVec2(90.f, 0.f))) {
                const EntityEntry& e = entityEntries[filteredIdx[selectedIdx]];
                const bool nowFrozen = ToggleEntityFreeze(e.pSoul);
                lastAction = nowFrozen
                                 ? ("Frozen '" + e.name + "'.")
                                 : ("Unfrozen '" + e.name + "'.");
            }

            if (!hasSelection)
                ig::EndDisabled( );

            if (!lastAction.empty( )) {
                const bool fail = (lastAction.find("failed") != std::string::npos ||
                                   lastAction.find("Failed") != std::string::npos);
                ig::Spacing( );
                ig::TextColored(fail ? ImVec4(1.f, 0.35f, 0.35f, 1.f) : ImVec4(0.55f, 0.85f, 0.55f, 1.f),
                                "%s", lastAction.c_str( ));
            }
        }

        static void RenderDebugMenu( ) {
            RenderDebugMenu_SEH( );
        }

        // ══════════════════════════════════════════════════════════════════════
        //  Log Reader
        // ══════════════════════════════════════════════════════════════════════
        struct LogReader {
            std::string content;
            double lastUpdateTime = 0.0;
            uint64_t lastFileSize = 0;
            char buffer[65536] = {0};
            bool autoScroll = true;
            bool updatedThisFrame = false;

            void Update( ) {
                updatedThisFrame = false;
                double now = ig::GetTime( );
                if (now - lastUpdateTime < 0.5)
                    return;
                lastUpdateTime = now;

                char path[MAX_PATH];
                if (!GetModuleFileNameA(NULL, path, MAX_PATH))
                    return;
                std::string s(path);
                size_t binPos = s.find("\\Bin\\");
                if (binPos == std::string::npos)
                    binPos = s.find("/Bin/");
                if (binPos == std::string::npos)
                    return;

                std::string logPath = s.substr(0, binPos) + "\\kcd.log";

                HANDLE hFile = CreateFileA(logPath.c_str( ), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    LARGE_INTEGER size;
                    if (GetFileSizeEx(hFile, &size)) {
                        if ((uint64_t)size.QuadPart < lastFileSize) {
                            content.clear( );
                            lastFileSize = 0;
                        }
                        if ((uint64_t)size.QuadPart > lastFileSize) {
                            uint64_t bytesToRead = size.QuadPart - lastFileSize;
                            if (bytesToRead > sizeof(buffer) - 1) {
                                lastFileSize = size.QuadPart - (sizeof(buffer) - 1);
                                bytesToRead = sizeof(buffer) - 1;
                                content.clear( );
                            }

                            LARGE_INTEGER liPos;
                            liPos.QuadPart = lastFileSize;
                            SetFilePointerEx(hFile, liPos, NULL, FILE_BEGIN);

                            DWORD bytesRead = 0;
                            std::vector<char> temp(bytesToRead + 1, 0);
                            if (ReadFile(hFile, temp.data( ), (DWORD)bytesToRead, &bytesRead, NULL) && bytesRead > 0) {
                                content.append(temp.data( ), bytesRead);
                                lastFileSize += bytesRead;
                                if (content.size( ) >= sizeof(buffer)) {
                                    content = content.substr(content.size( ) - sizeof(buffer) + 1);
                                }
                                size_t lenToCopy = content.size( );
                                if (lenToCopy > sizeof(buffer) - 1)
                                    lenToCopy = sizeof(buffer) - 1;
                                memcpy(buffer, content.c_str( ), lenToCopy);
                                buffer[lenToCopy] = '\0';
                                updatedThisFrame = true;
                            }
                        }
                    }
                    CloseHandle(hFile);
                }
            }
        };

        static LogReader s_logReader;

        // ══════════════════════════════════════════════════════════════════════
        //  Lua Executor panel
        // ══════════════════════════════════════════════════════════════════════
        static char s_luaBuffer[8192] = "System.LogAlways('Hello from the Menu Executor!')\n";
        static std::string s_luaLastAction = "";

        static int s_lastSelStart = 0;
        static int s_lastSelEnd = 0;
        static int s_lastBufTextLen = 0;

        static bool SafeCallExecuteLua(const char* code, int64_t* outResult) {
            SEH_TRY {
                HMODULE hModule = GetModuleHandleA("WHGame.dll");
                uintptr_t executeLuaAddr = hModule ? (reinterpret_cast<uintptr_t>(hModule) + 0x4D46E4) : 0x1804d46e4;

                typedef int64_t(__fastcall * ExecuteLuaBufferFunc)(void*, const char*, size_t, const char*, void*);
                ExecuteLuaBufferFunc executeLua = reinterpret_cast<ExecuteLuaBufferFunc>(executeLuaAddr);

                *outResult = executeLua(
                    reinterpret_cast<void*>(hk::CScriptSystem),
                    code,
                    strlen(code),
                    "Menu_Injected",
                    nullptr);
                return true;
            } SEH_EXCEPT {
                return false;
            }
        }

        static void ExecuteLuaCode(const char* code) {
            if (!hk::CScriptSystem) {
                s_luaLastAction = "Error: hk::CScriptSystem is null! Load into a game first.";
                return;
            }

            int64_t result = 0;
            if (SafeCallExecuteLua(code, &result)) {
                s_luaLastAction = "Executed successfully.";
            } else {
                s_luaLastAction = "Exception caught! Crash prevented.";
            }
        }

        static std::string ReadClipboardUtf8( ) {
            std::string out;
            if (!OpenClipboard(nullptr))
                return out;

            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(hData));
                if (wide) {
                    int needed = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
                    if (needed > 1) {
                        out.resize(static_cast<size_t>(needed - 1));
                        WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data( ), needed, nullptr, nullptr);
                    }
                    GlobalUnlock(hData);
                }
            } else {
                hData = GetClipboardData(CF_TEXT);
                if (hData) {
                    const char* ansi = static_cast<const char*>(GlobalLock(hData));
                    if (ansi)
                        out = ansi;
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard( );

            if (!out.empty( )) {
                std::string clean;
                clean.reserve(out.size( ));
                for (char c : out)
                    if (c != '\r')
                        clean.push_back(c);
                out.swap(clean);
            }
            return out;
        }

        static bool WriteClipboardUtf8(const char* text, size_t len) {
            if (!text || len == 0)
                return false;

            const int wideLen = MultiByteToWideChar(CP_UTF8, 0, text, (int)len, nullptr, 0);
            if (wideLen <= 0)
                return false;

            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wideLen + 1) * sizeof(wchar_t));
            if (!hMem)
                return false;

            wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem));
            if (!dst) {
                GlobalFree(hMem);
                return false;
            }
            MultiByteToWideChar(CP_UTF8, 0, text, (int)len, dst, wideLen);
            dst[wideLen] = L'\0';
            GlobalUnlock(hMem);

            if (!OpenClipboard(nullptr)) {
                GlobalFree(hMem);
                return false;
            }
            EmptyClipboard( );
            if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
                GlobalFree(hMem);
                CloseClipboard( );
                return false;
            }
            CloseClipboard( );
            return true;
        }

        static bool InsertClipboardAtCursor(char* buffer, size_t bufferSize,
                                            int* cursorPos, int* selStart, int* selEnd,
                                            int* bufTextLen) {
            if (!buffer || bufferSize == 0 || !cursorPos)
                return false;
            const std::string clip = ReadClipboardUtf8( );
            if (clip.empty( ))
                return false;

            const size_t curBufLen = strlen(buffer);
            int cur = (*cursorPos >= 0) ? *cursorPos : (int)curBufLen;
            if (cur > (int)curBufLen)
                cur = (int)curBufLen;

            int a = (selStart ? *selStart : cur);
            int b = (selEnd ? *selEnd : cur);
            if (a > b)
                std::swap(a, b);
            if (a < 0)
                a = 0;
            if (b > (int)curBufLen)
                b = (int)curBufLen;
            if (!(b > a)) {
                a = cur;
                b = cur;
            }

            const size_t remaining = curBufLen - (size_t)(b - a);
            if (remaining + 1 >= bufferSize)
                return false;

            const size_t room = bufferSize - 1 - remaining;
            size_t insLen = clip.size( );
            if (insLen > room)
                insLen = room;
            if (insLen == 0)
                return false;

            memmove(buffer + a + insLen, buffer + b, curBufLen - (size_t)b + 1);
            memcpy(buffer + a, clip.data( ), insLen);

            const int newCursor = a + (int)insLen;
            *cursorPos = newCursor;
            if (selStart)
                *selStart = newCursor;
            if (selEnd)
                *selEnd = newCursor;
            if (bufTextLen)
                *bufTextLen = (int)(remaining + insLen);
            return true;
        }

        static int s_cursorBytePos = 0;
        static int s_pendingCursorPos = -1;
        static bool s_pendingRefocus = false;

        // ──────────────────────────────────────────────────────────────────────
        //  Autocomplete Context Models
        // ──────────────────────────────────────────────────────────────────────
        struct CompletionContext {
            const char* parent;
            char separator;
            std::vector<const char*> suggestions;
        };

        // KCD2 Object / Module structure for intelligent Context completion
        static const CompletionContext kContexts[] = {
            // Player / Entities properties (.)
            {"player", '.', {"inventory", "soul", "actor", "human", "this", "id", "class"}},
            {"e", '.', {"inventory", "soul", "actor", "human", "this", "id", "class"}},
            {"npc", '.', {"inventory", "soul", "actor", "human", "this", "id", "class"}},
            {"target", '.', {"inventory", "soul", "actor", "human", "this", "id", "class"}},

            // Player / Entities methods (:)
            {"player", ':', {"GetWorldPos", "GetPos", "GetAngles", "GetWorldDir", "SetWorldPos", "GetPhysicalStats", "AwakePhysics", "AddImpulse", "SetColliderMode", "SetPlayerHorse", "GetPlayerHorse", "GetHorseId", "HorseInspect", "Mount", "ForceDismount", "GetName", "Reset"}},
            {"e", ':', {"GetWorldPos", "GetPos", "GetAngles", "GetWorldDir", "SetWorldPos", "GetPhysicalStats", "AwakePhysics", "AddImpulse", "SetColliderMode", "GetName", "Reset"}},
            {"npc", ':', {"GetWorldPos", "GetPos", "GetAngles", "GetWorldDir", "SetWorldPos", "GetPhysicalStats", "AwakePhysics", "AddImpulse", "SetColliderMode", "GetName", "Reset"}},
            {"target", ':', {"GetWorldPos", "GetPos", "GetAngles", "GetWorldDir", "SetWorldPos", "GetPhysicalStats", "AwakePhysics", "AddImpulse", "SetColliderMode", "GetName", "Reset"}},

            // Object specific methods (:)
            {"soul", ':', {"SetState", "GetState", "GetStatLevel", "GetDerivedStat", "HasPerk", "AddPerk", "RemovePerk", "AddAllCodexPerks", "AdvanceToStatLevel", "GetSkillLevel", "AdvanceToSkillLevel", "DealDamage", "HealBleeding", "AddBuff", "RemoveAllBuffsByGuid", "GetGender", "GetFactionID", "GetSocialClass", "Revive"}},
            {"actor", ':', {"WashDirtAndBlood", "WashItems", "RequestItemExchange", "GetMaxHealth", "GetHealth", "Revive", "InitialSetup", "GetCurrentAnimationState"}},
            {"inventory", ':', {"GetCountOfClass", "GetInventoryTable", "CreateItem", "DeleteItemOfClass", "RemoveAllItems"}},

            // System Modules / Namespaces (.)
            {"System", '.', {"ExecuteCommand", "GetEntity", "GetEntityByName", "ScanDirectory", "SpawnEntity", "RemoveEntity", "GetViewCameraDir", "GetEntitiesInSphere", "GetEntities", "GetEntitiesByClass", "SetCVar", "Log", "LogAlways"}},
            {"Game", '.', {"ShowItemsTransfer", "IsLoadingEngineSaveGame", "SaveGameViaResting"}},
            {"Script", '.', {"SetTimer", "KillTimer", "ReloadScripts", "SetTimerForFunction"}},
            {"XGenAIModule", '.', {"GetEntityByWUID", "GetMyWUID", "GetOwner", "FindLinks", "SendMessageToEntity", "SpawnEntity"}},
            {"Calendar", '.', {"GetWorldTime", "GetWorldTimeRatio", "IsWorldTimePaused", "SetWorldTime", "SetWorldTimeRatio", "IsFakedTimeOfDay", "SetFakeTimeOfDay", "GetWorldHourOfDay", "UnfakeTimeOfDay"}},
            {"EnvironmentModule", '.', {"BlendTimeOfDay"}},
            {"Minigame", '.', {"StartLockPicking", "StartDice"}},
            {"Physics", '.', {"RayWorldIntersection"}},
            {"ItemManager", '.', {"GetItem", "GetItemOwner", "GetItemName", "GetItemUIName", "IsItemOversized"}},
            {"ItemSystem", '.', {"GetPackItemByIndex", "GetPackNumItems", "GetPackPrimaryItem", "GiveItem", "GiveItemPack", "Reset", "SerializePlayerLTLInfo", "SetActorItem", "SetActorItemByName"}},
            {"ItemUtils", '.', {"AddMoneyToInventory", "CheckMoneyAmount", "CreateInvItem", "DeliverItemByPlayer", "GiveItemToPlayer", "HasItem", "IsItemInInventory", "IsNullWeaponGuid", "MoneyTransaction", "RemoveInvItem", "RemoveMoneyFromInventory"}},
        };

        // ──────────────────────────────────────────────────────────────────────
        //  Autocomplete
        // ──────────────────────────────────────────────────────────────────────
        static struct LuaCompletionState {
            bool available = false;
            char suffix[64] = {0};
            char fullWord[64] = {0};
            int wordStart = 0;
            int wordLen = 0;
        } s_completion;

        static int LuaInputTextCallback(ImGuiInputTextCallbackData* data) {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
                if (data->EventChar == '\t' &&
                    s_completion.available &&
                    s_completion.suffix[0] != '\0') {
                    data->InsertChars(data->CursorPos, s_completion.suffix);
                    s_completion.available = false;
                    s_completion.suffix[0] = '\0';
                    return 1;
                }
                return 0;
            }

            if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
                if (s_pendingCursorPos >= 0 && s_pendingCursorPos <= data->BufTextLen) {
                    data->CursorPos = s_pendingCursorPos;
                    data->SelectionStart = data->SelectionEnd = s_pendingCursorPos;
                    s_pendingCursorPos = -1;
                }
                s_cursorBytePos = data->CursorPos;
                s_lastSelStart = data->SelectionStart;
                s_lastSelEnd = data->SelectionEnd;
                s_lastBufTextLen = data->BufTextLen;
            }
            return 0;
        }

        // ──────────────────────────────────────────────────────────────────────
        //  Lua tokenizer + syntax-highlight overlay
        // ──────────────────────────────────────────────────────────────────────

        enum LuaTokenKind : unsigned char {
            LTK_NONE,
            LTK_KEYWORD,
            LTK_NUMBER,
            LTK_STRING,
            LTK_COMMENT,
            LTK_OPERATOR,
            LTK_BUILTIN_OBJ,
            LTK_BUILTIN_FUNC,
        };

        struct LuaToken {
            int start;
            int length;
            LuaTokenKind kind;
        };

        static const char* const kLuaKeywords[] = {
            "and", "break", "do", "else", "elseif", "end",
            "false", "for", "function", "goto", "if", "in",
            "local", "nil", "not", "or", "repeat", "return",
            "then", "true", "until", "while"};

        static const char* const kLuaBuiltinObjs[] = {
            "string", "math", "table", "io", "os", "self", "_G",
            "System", "Game", "Script", "UIAction", "player", "e", "target", "npc",
            "XGenAIModule", "ItemManager", "ItemSystem", "ItemUtils",
            "Physics", "Calendar", "EnvironmentModule", "Minigame",
            "VectorUtils", "RPG", "Stash", "AnimDoor", "BasicAIActions",
            "ent_all", "SCANDIR_FILES", "__null",
            "soul", "human", "inventory", "actor", "this", "id", "class",
            "AIMovementAbility", "walkSpeed", "runSpeed", "sprintSpeed",
            "StatCap", "SkillCap"};

        static const char* const kLuaBuiltinFuncs[] = {
            "print", "tostring", "tonumber", "ipairs", "pairs", "type",
            "pcall", "xpcall", "require", "rawget", "rawset",
            "setmetatable", "getmetatable", "select", "unpack", "error", "assert",
            "ExecuteCommand", "GetEntity", "GetEntityByName", "ScanDirectory",
            "SpawnEntity", "RemoveEntity", "GetViewCameraDir", "GetEntitiesInSphere",
            "GetEntities", "GetEntitiesByClass", "SetCVar", "Log", "LogAlways",
            "AddBuff", "RemoveAllBuffsByGuid", "HealBleeding", "SetState", "GetState",
            "GetStatLevel", "GetDerivedStat", "HasPerk", "AddPerk", "RemovePerk",
            "AddAllCodexPerks", "AdvanceToStatLevel", "GetSkillLevel", "AdvanceToSkillLevel",
            "DealDamage", "SetPlayerHorse", "GetPlayerHorse", "GetHorseId", "HorseInspect",
            "Mount", "ForceDismount", "WashDirtAndBlood", "WashItems", "RequestItemExchange",
            "GetMaxHealth", "GetHealth", "GetWorldPos", "GetPos", "GetAngles", "GetWorldDir", "SetWorldPos",
            "GetPhysicalStats", "AwakePhysics", "AddImpulse", "SetColliderMode",
            "Revive", "InitialSetup", "ReviveInEditor", "GetGender", "GetFactionID", "GetSocialClass",
            "GetName", "Reset", "GetCurrentAnimationState",
            "GetCountOfClass", "GetInventoryTable", "CreateItem", "DeleteItemOfClass",
            "RemoveAllItems", "GetItem", "GetItemOwner", "GetItemName", "GetItemUIName",
            "IsItemOversized", "GetPackItemByIndex", "GetPackNumItems", "GetPackPrimaryItem",
            "GiveItem", "GiveItemPack", "SerializePlayerLTLInfo", "SetActorItem",
            "SetActorItemByName", "AddMoneyToInventory", "CheckMoneyAmount", "CreateInvItem",
            "DeliverItemByPlayer", "GiveItemToPlayer", "HasItem", "IsItemInInventory",
            "IsNullWeaponGuid", "MoneyTransaction", "RemoveInvItem", "RemoveMoneyFromInventory",
            "GetEntityByWUID", "GetMyWUID", "GetOwner", "FindLinks", "SendMessageToEntity",
            "ShowItemsTransfer", "IsLoadingEngineSaveGame", "SaveGameViaResting",
            "RayWorldIntersection", "GetWorldTime", "GetWorldTimeRatio", "IsWorldTimePaused",
            "SetWorldTime", "SetWorldTimeRatio", "IsFakedTimeOfDay", "SetFakeTimeOfDay",
            "GetWorldHourOfDay", "UnfakeTimeOfDay", "BlendTimeOfDay",
            "SetTimer", "KillTimer", "ReloadScripts", "SetTimerForFunction",
            "StartLockPicking", "StartDice", "Scale", "OnUsedHold", "Lockpick",
            "OnPickpocketing", "Unlock", "Open", "IsOnKeySide", "OnUsed"};

        static bool TokenMatches(const char* src, int start, int len, const char* word) {
            return (int)strlen(word) == len && memcmp(src + start, word, len) == 0;
        }

        static void TokenizeLua(const char* src, std::vector<LuaToken>& out) {
            out.clear( );
            if (!src)
                return;

            int i = 0;
            while (src[i]) {
                const char c = src[i];

                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    ++i;
                    continue;
                }

                if (c == '-' && src[i + 1] == '-') {
                    const int start = i;
                    if (src[i + 2] == '[' && src[i + 3] == '[') {
                        i += 4;
                        while (src[i] && !(src[i] == ']' && src[i + 1] == ']'))
                            ++i;
                        if (src[i])
                            i += 2;
                    } else {
                        while (src[i] && src[i] != '\n')
                            ++i;
                    }
                    out.push_back({start, i - start, LTK_COMMENT});
                    continue;
                }

                if (c == '"' || c == '\'') {
                    const int start = i;
                    const char q = c;
                    ++i;
                    while (src[i] && src[i] != q) {
                        if (src[i] == '\\' && src[i + 1])
                            i += 2;
                        else if (src[i] == '\n')
                            break;
                        else
                            ++i;
                    }
                    if (src[i] == q)
                        ++i;
                    out.push_back({start, i - start, LTK_STRING});
                    continue;
                }

                if (c == '[' && src[i + 1] == '[') {
                    const int start = i;
                    i += 2;
                    while (src[i] && !(src[i] == ']' && src[i + 1] == ']'))
                        ++i;
                    if (src[i])
                        i += 2;
                    out.push_back({start, i - start, LTK_STRING});
                    continue;
                }

                const bool isDigit = (c >= '0' && c <= '9');
                if (isDigit || (c == '.' && src[i + 1] >= '0' && src[i + 1] <= '9')) {
                    const int start = i;
                    if (c == '0' && (src[i + 1] == 'x' || src[i + 1] == 'X')) {
                        i += 2;
                        while ((src[i] >= '0' && src[i] <= '9') ||
                               (src[i] >= 'a' && src[i] <= 'f') ||
                               (src[i] >= 'A' && src[i] <= 'F'))
                            ++i;
                    } else {
                        bool hasDot = false, hasExp = false;
                        while (src[i]) {
                            const char ch = src[i];
                            if (ch >= '0' && ch <= '9') {
                                ++i;
                            } else if (ch == '.' && !hasDot && !hasExp) {
                                hasDot = true;
                                ++i;
                            } else if ((ch == 'e' || ch == 'E') && !hasExp) {
                                hasExp = true;
                                ++i;
                                if (src[i] == '+' || src[i] == '-')
                                    ++i;
                            } else
                                break;
                        }
                    }
                    out.push_back({start, i - start, LTK_NUMBER});
                    continue;
                }

                const bool isAlpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
                if (isAlpha) {
                    const int start = i;
                    while (src[i] && ((src[i] >= 'a' && src[i] <= 'z') ||
                                      (src[i] >= 'A' && src[i] <= 'Z') ||
                                      (src[i] >= '0' && src[i] <= '9') ||
                                      src[i] == '_'))
                        ++i;
                    const int len = i - start;

                    LuaTokenKind kind = LTK_NONE;
                    for (const char* kw : kLuaKeywords) {
                        if (TokenMatches(src, start, len, kw)) {
                            kind = LTK_KEYWORD;
                            break;
                        }
                    }
                    if (kind == LTK_NONE) {
                        for (const char* obj : kLuaBuiltinObjs) {
                            if (TokenMatches(src, start, len, obj)) {
                                kind = LTK_BUILTIN_OBJ;
                                break;
                            }
                        }
                    }
                    if (kind == LTK_NONE) {
                        for (const char* func : kLuaBuiltinFuncs) {
                            if (TokenMatches(src, start, len, func)) {
                                kind = LTK_BUILTIN_FUNC;
                                break;
                            }
                        }
                    }

                    if (kind != LTK_NONE)
                        out.push_back({start, len, kind});
                    continue;
                }

                if (strchr("+-*/%^#=~<>(){}[];:,.&|", c)) {
                    const int start = i;
                    ++i;
                    if ((c == '=' || c == '~' || c == '<' || c == '>') && src[i] == '=')
                        ++i;
                    else if (c == '.' && src[i] == '.') {
                        ++i;
                        if (src[i] == '.')
                            ++i;
                    } else if (c == ':' && src[i] == ':')
                        ++i;
                    out.push_back({start, i - start, LTK_OPERATOR});
                    continue;
                }

                ++i;
            }
        }

        static ImU32 LuaTokenColor(LuaTokenKind k) {
            switch (k) {
                case LTK_KEYWORD:
                    return IM_COL32(207, 142, 247, 255); // purple
                case LTK_NUMBER:
                    return IM_COL32(181, 206, 168, 255); // pale green
                case LTK_STRING:
                    return IM_COL32(206, 145, 120, 255); // orange
                case LTK_COMMENT:
                    return IM_COL32(106, 153, 85, 255); // forest green
                case LTK_OPERATOR:
                    return IM_COL32(200, 200, 200, 255); // light gray
                case LTK_BUILTIN_OBJ:
                    return IM_COL32(78, 201, 176, 255); // teal (Objects / Namespaces)
                case LTK_BUILTIN_FUNC:
                    return IM_COL32(220, 220, 170, 255); // pale yellow (Functions / Methods)
                default:
                    return IM_COL32(220, 220, 220, 255); // off-white
            }
        }

        static float LuaTabAdvance(float xRel, float spaceW) {
            const float tabW = spaceW * 4.0f;
            const int n = (int)(xRel / tabW);
            return (float)(n + 1) * tabW - xRel;
        }

        static void RenderHighlightedLua(ImDrawList* dl, ImVec2 startPos,
                                         const char* text,
                                         const std::vector<LuaToken>& tokens) {
            if (!text || !*text)
                return;

            const int len = (int)strlen(text);
            std::vector<unsigned char> kindOf(len, (unsigned char)LTK_NONE);
            for (const LuaToken& t : tokens) {
                for (int j = 0; j < t.length && t.start + j < len; ++j)
                    kindOf[t.start + j] = (unsigned char)t.kind;
            }

            ImFont* font = ig::GetFont( );
            const float fontSize = font->FontSize;
            const float lineHeight = ig::GetTextLineHeight( );
            const float spaceW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, " ").x;

            float x = startPos.x;
            float y = startPos.y;

            int i = 0;
            while (i < len) {
                const char c = text[i];
                if (c == '\n') {
                    x = startPos.x;
                    y += lineHeight;
                    ++i;
                    continue;
                }
                if (c == '\r') {
                    ++i;
                    continue;
                }
                if (c == '\t') {
                    x += LuaTabAdvance(x - startPos.x, spaceW);
                    ++i;
                    continue;
                }

                const int runStart = i;
                const unsigned char k = kindOf[i];
                while (i < len && text[i] != '\n' && text[i] != '\r' &&
                       text[i] != '\t' && kindOf[i] == k)
                    ++i;

                const char* s = text + runStart;
                const char* e = text + i;
                dl->AddText(font, fontSize, ImVec2(x, y),
                            LuaTokenColor((LuaTokenKind)k), s, e);
                x += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, s, e).x;
            }
        }

        static ImVec2 LuaCursorScreenPos(ImVec2 textStart, const char* text,
                                         int byteOffset, float lineHeight) {
            ImFont* font = ig::GetFont( );
            const float fontSize = font->FontSize;
            const float spaceW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, " ").x;

            float x = textStart.x;
            float y = textStart.y;
            int i = 0;
            const int len = text ? (int)strlen(text) : 0;
            const int stop = (byteOffset < len) ? byteOffset : len;

            while (i < stop) {
                const char c = text[i];
                if (c == '\n') {
                    x = textStart.x;
                    y += lineHeight;
                    ++i;
                    continue;
                }
                if (c == '\r') {
                    ++i;
                    continue;
                }
                if (c == '\t') {
                    x += LuaTabAdvance(x - textStart.x, spaceW);
                    ++i;
                    continue;
                }

                const int runStart = i;
                while (i < stop && text[i] != '\n' && text[i] != '\r' && text[i] != '\t')
                    ++i;
                x += font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f,
                                         text + runStart, text + i)
                         .x;
            }
            return ImVec2(x, y);
        }

        // ──────────────────────────────────────────────────────────────────────
        //  Autocomplete helpers
        // ──────────────────────────────────────────────────────────────────────

        static bool IsIdentChar(char c) {
            return (c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') ||
                   c == '_';
        }

        static bool IsIdentStartChar(char c) {
            return (c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z') ||
                   c == '_';
        }

        static int FindCurrentWordStart(const char* buf, int cursor) {
            if (!buf || cursor <= 0)
                return cursor;
            if (!IsIdentChar(buf[cursor - 1]))
                return cursor;
            if (buf[cursor] != '\0' && IsIdentChar(buf[cursor]))
                return cursor;

            int s = cursor;
            while (s > 0 && IsIdentChar(buf[s - 1]))
                --s;
            if (s < cursor && !IsIdentStartChar(buf[s]))
                return cursor;
            return s;
        }

        static const std::vector<const char*>& GetCompletionDictionary( ) {
            static std::vector<const char*> dict;
            if (!dict.empty( ))
                return dict;
            for (const char* kw : kLuaKeywords)
                dict.push_back(kw);
            for (const char* o : kLuaBuiltinObjs)
                dict.push_back(o);
            for (const char* f : kLuaBuiltinFuncs)
                dict.push_back(f);

            std::sort(dict.begin( ), dict.end( ),
                      [](const char* a, const char* b) { return strcmp(a, b) < 0; });
            return dict;
        }

        static void UpdateLuaCompletion( ) {
            s_completion.available = false;
            s_completion.suffix[0] = '\0';
            s_completion.fullWord[0] = '\0';
            s_completion.wordStart = s_cursorBytePos;
            s_completion.wordLen = 0;

            const int wordStart = FindCurrentWordStart(s_luaBuffer, s_cursorBytePos);
            const int wordLen = s_cursorBytePos - wordStart;

            char contextObj[64] = {0};
            char sep = 0;

            if (wordStart > 0 && (s_luaBuffer[wordStart - 1] == '.' || s_luaBuffer[wordStart - 1] == ':')) {
                sep = s_luaBuffer[wordStart - 1];
                int ctxEnd = wordStart - 1;
                int ctxStart = FindCurrentWordStart(s_luaBuffer, ctxEnd);
                if (ctxStart < ctxEnd) {
                    int ctxLen = ctxEnd - ctxStart;
                    if (ctxLen < (int)sizeof(contextObj)) {
                        memcpy(contextObj, s_luaBuffer + ctxStart, ctxLen);
                        contextObj[ctxLen] = '\0';
                    }
                }
            }

            const char* word = s_luaBuffer + wordStart;

            // 1. Contextual Objects/Properties (Dot/Colon syntax)
            if (sep != 0) {
                if (contextObj[0] != '\0') {
                    bool foundContext = false;
                    for (const auto& ctx : kContexts) {
                        if (ctx.separator == sep && strcmp(ctx.parent, contextObj) == 0) {
                            foundContext = true;
                            for (const char* cand : ctx.suggestions) {
                                const int candLen = (int)strlen(cand);
                                if (candLen < wordLen)
                                    continue;

                                if (wordLen > 0 && strncmp(cand, word, (size_t)wordLen) != 0)
                                    continue;

                                const int suffixLen = candLen - wordLen;
                                if (suffixLen >= (int)sizeof(s_completion.suffix))
                                    continue;

                                if (suffixLen > 0) {
                                    memcpy(s_completion.suffix, cand + wordLen, (size_t)suffixLen);
                                }
                                s_completion.suffix[suffixLen] = '\0';

                                const int fullCopy = (candLen < (int)sizeof(s_completion.fullWord) - 1)
                                                         ? candLen
                                                         : (int)sizeof(s_completion.fullWord) - 1;
                                memcpy(s_completion.fullWord, cand, (size_t)fullCopy);
                                s_completion.fullWord[fullCopy] = '\0';

                                s_completion.wordStart = wordStart;
                                s_completion.wordLen = wordLen;
                                s_completion.available = true;
                                return;
                            }
                        }
                    }
                    if (foundContext) {
                        return; // Found a valid context, but no matches. Don't fall through to global.
                    }
                }
                return; // User typed '.' or ':' but it didn't match a known context. Don't fall through.
            }

            if (wordLen <= 0)
                return;

            // 2. Global Dictionary Completion (Fallback)
            const auto& dict = GetCompletionDictionary( );
            for (const char* cand : dict) {
                const int candLen = (int)strlen(cand);
                if (candLen <= wordLen)
                    continue;
                if (strncmp(cand, word, (size_t)wordLen) != 0)
                    continue;

                const int suffixLen = candLen - wordLen;
                if (suffixLen <= 0 || suffixLen >= (int)sizeof(s_completion.suffix))
                    continue;
                memcpy(s_completion.suffix, cand + wordLen, (size_t)suffixLen);
                s_completion.suffix[suffixLen] = '\0';

                const int fullCopy = (candLen < (int)sizeof(s_completion.fullWord) - 1)
                                         ? candLen
                                         : (int)sizeof(s_completion.fullWord) - 1;
                memcpy(s_completion.fullWord, cand, (size_t)fullCopy);
                s_completion.fullWord[fullCopy] = '\0';

                s_completion.wordStart = wordStart;
                s_completion.wordLen = wordLen;
                s_completion.available = true;
                return;
            }
        }

        // ──────────────────────────────────────────────────────────────────────
        //  Example scripts
        // ──────────────────────────────────────────────────────────────────────

        struct LuaExample {
            const char* label;
            const char* code;
        };

        static const LuaExample kLuaExamples[] = {
            {"Spawn Horse",
             "-- Spawn a horse 3m from the player\n"
             "local pos = player:GetWorldPos()\n"
             "pos.x = pos.x + 3.0\n"
             "local e = System.SpawnEntity({\n"
             "    class    = \"Horse\",\n"
             "    position = pos\n"
             "})\n"
             "if e then\n"
             "    System.LogAlways(\"Spawned: \" .. tostring(e:GetName()))\n"
             "else\n"
             "    System.LogAlways(\"Spawn failed.\")\n"
             "end\n"},
            {"Spawn Boar",
             "-- Drop a boar 4m in front for combat practice\n"
             "local pos = player:GetWorldPos()\n"
             "pos.x = pos.x + 4.0\n"
             "local e = System.SpawnEntity({ class = \"Boar\", position = pos })\n"
             "System.LogAlways(\"Target: \" .. tostring(e and e:GetName() or \"nil\"))\n"},
            {"Heal Player",
             "-- Top up HP and stamina\n"
             "player.soul:SetState(\"health\",  1000)\n"
             "player.soul:SetState(\"stamina\", 1000)\n"
             "System.LogAlways(\"Healed.\")\n"},
            {"Player Info",
             "-- Dump current location and name\n"
             "local p = player:GetWorldPos()\n"
             "System.LogAlways(string.format(\"Pos: %.1f, %.1f, %.1f\", p.x, p.y, p.z))\n"
             "System.LogAlways(\"Name: \" .. tostring(player:GetName()))\n"},
            {"Step Forward",
             "-- Teleport 5m along the player's facing direction\n"
             "local pos = player:GetWorldPos()\n"
             "local ang = player:GetAngles()\n"
             "local d   = 5.0\n"
             "pos.x = pos.x + math.cos(ang.z) * d\n"
             "pos.y = pos.y + math.sin(ang.z) * d\n"
             "player:SetWorldPos(pos)\n"},
            {"List Nearby",
             "-- Enumerate entities within 15m\n"
             "local hits = System.GetEntitiesInSphere(player:GetWorldPos(), 15.0)\n"
             "System.LogAlways(\"Found \" .. tostring(#hits) .. \" entities\")\n"
             "for i, e in ipairs(hits) do\n"
             "    System.LogAlways(\"  \" .. i .. \": \" .. tostring(e:GetName()))\n"
             "end\n"},
            {"Save Game",
             "-- Quick save without an inn or schnapps\n"
             "if not Game.IsLoadingEngineSaveGame() then\n"
             "    Game.SaveGameViaResting()\n"
             "    System.LogAlways(\"Saved.\")\n"
             "end\n"},
            {"Add Item",
             "-- Add an item via ID to the player's inventory\n"
             "local itemId = \"2264f217-590e-4c0f-a4c6-f50c6532b9f6\" -- Your Item ID\n"
             "player.inventory:CreateItem(itemId, 1, 1)\n"
             "System.LogAlways(\"Item added.\")\n"},
            {"Remove Item",
             "-- Remove an item via ID from the player's inventory\n"
             "local guid = \"2264f217-590e-4c0f-a4c6-f50c6532b9f6\" --Your Item ID\n"
             "player.inventory : DeleteItemOfClass(guid, 1)"},
            {"Play Dice!",
             "-- Play Dice with the closest entity to your player\n"
             "local function dist2(a, b)\n"
             "    local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z\n"
             "    return dx*dx + dy*dy + dz*dz\n"
             "end\n"
             "\n"
             "local function findNearest(pos, radius, predicate)\n"
             "    local best, bestD = nil, 1e9\n"
             "    for _, e in pairs(System.GetEntities(pos, radius) or {}) do\n"
             "        if predicate(e) then\n"
             "            local d = dist2(e:GetPos(), pos)\n"
             "            if d < bestD then best, bestD = e, d end\n"
             "        end\n"
             "    end\n"
             "    return best, bestD\n"
             "end\n"
             "\n"
             "local ok, err = xpcall(function()\n"
             "    local pos = player:GetPos()\n"
             "\n"
             "    local npc = findNearest(pos, 50, function(e)\n"
             "        if e.id == player.id then return false end\n"
             "        return e.soul ~= nil   -- anything with a soul is an NPC\n"
             "    end)\n"
             "    if not npc then System.LogAlways(\"No NPC within 50m\"); return end\n"
             "\n"
             "    local tbl = findNearest(pos, 100, function(e)\n"
             "        local c = tostring(e.class or \"\"):lower()\n"
             "        local n = tostring(e:GetName() or \"\"):lower()\n"
             "        return c:find(\"dice\") or n:find(\"dice\")\n"
             "    end)\n"
             "    if not tbl then System.LogAlways(\"No dice table within 100m\"); return end\n"
             "\n"
             "    System.LogAlways(\"NPC:   \" .. tostring(npc:GetName()))\n"
             "    System.LogAlways(\"Table: \" .. tostring(tbl:GetName()))\n"
             "\n"
             "    Minigame.StartDice(tbl.id, player.id, npc.id, { wager = 50, targetScore = 2000, useBadges = false })\n"
             "end, tostring)\n"
             "\n"
             "if not ok then System.LogAlways(\"ERR: \" .. tostring(err)) end\n"},
            {"Inspect Nearest",
             "-- Dump info about the closest NPC\n"
             "local pos = player:GetPos()\n"
             "local best, bestD = nil, 1e9\n"
             "for _, e in pairs(System.GetEntities(pos, 30) or {}) do\n"
             "    if e.id ~= player.id and e.soul then\n"
             "        local ep = e:GetPos()\n"
             "        local d  = (ep.x-pos.x)^2 + (ep.y-pos.y)^2 + (ep.z-pos.z)^2\n"
             "        if d < bestD then best, bestD = e, d end\n"
             "    end\n"
             "end\n"
             "if best then\n"
             "    System.LogAlways(\"Name:  \" .. tostring(best:GetName()))\n"
             "    System.LogAlways(\"Class: \" .. tostring(best.class))\n"
             "    System.LogAlways(\"ID:    \" .. tostring(best.id))\n"
             "    System.LogAlways(\"HP:    \" .. tostring(best.soul:GetState(\"health\")))\n"
             "    System.LogAlways(\"Dist:  \" .. string.format(\"%.1fm\", math.sqrt(bestD)))\n"
             "else\n"
             "    System.LogAlways(\"No NPC within 30m.\")\n"
             "end\n"},
            {"Give Item to Nearest",
             "-- Gift an item to the closest NPC\n"
             "local itemId = \"2264f217-590e-4c0f-a4c6-f50c6532b9f6\" -- Your Item ID\n"
             "local pos = player:GetPos()\n"
             "local best, bestD = nil, 1e9\n"
             "for _, e in pairs(System.GetEntities(pos, 30) or {}) do\n"
             "    if e.id ~= player.id and e.soul and e.inventory then\n"
             "        local ep = e:GetPos()\n"
             "        local d  = (ep.x-pos.x)^2 + (ep.y-pos.y)^2 + (ep.z-pos.z)^2\n"
             "        if d < bestD then best, bestD = e, d end\n"
             "    end\n"
             "end\n"
             "if best then\n"
             "    best.inventory:CreateItem(itemId, 1, 1)\n"
             "    System.LogAlways(\"Gave item to \" .. tostring(best:GetName()))\n"
             "else\n"
             "    System.LogAlways(\"No NPC within 30m.\")\n"
             "end\n"},
            {"Reload Scripts",
             "-- Hot-reload all Lua. Useful when iterating on mod files.\n"
             "Script.ReloadScripts()\n"
             "System.LogAlways(\"Scripts reloaded.\")\n"},
            {"Slow Time",
             "-- Bullet time. Run 'Normal Time' to restore.\n"
             "System.SetCVar(\"t_scale\", 0.3)\n"
             "System.LogAlways(\"t_scale = 0.3\")\n"},
            {"Normal Time",
             "-- Restore normal time flow\n"
             "System.SetCVar(\"t_scale\", 1.0)\n"
             "System.LogAlways(\"t_scale = 1.0\")\n"},
            {"Poll Anim State",
             "-- Poll animation state every 100ms for 30 seconds\n"
             "-- Hit the NPC manually during this window to capture reaction state names\n"
             "local pos = player:GetPos()\n"
             "local best, bestD = nil, 1e9\n"
             "for _, e in pairs(System.GetEntities(pos, 30) or {}) do\n"
             "    if e.id ~= player.id and e.soul then\n"
             "        local ep = e:GetPos()\n"
             "        local d = (ep.x-pos.x)^2 + (ep.y-pos.y)^2 + (ep.z-pos.z)^2\n"
             "        if d < bestD then best, bestD = e, d end\n"
             "    end\n"
             "end\n"
             "if not best then System.LogAlways(\"No NPC in 30m.\"); return end\n"
             "\n"
             "_anim_target  = best\n"
             "_anim_last    = \"\"\n"
             "_anim_ticks   = 300\n"
             "\n"
             "System.LogAlways(\"Watching: \" .. best:GetName() .. \" — HIT THEM NOW\")\n"
             "\n"
             "function anim_poll()\n"
             "    if _anim_ticks <= 0 then\n"
             "        System.LogAlways(\"[AnimPoll] Done.\")\n"
             "        return\n"
             "    end\n"
             "    _anim_ticks = _anim_ticks - 1\n"
             "\n"
             "    local ok, state = xpcall(function()\n"
             "        return _anim_target.actor:GetCurrentAnimationState()\n"
             "    end, tostring)\n"
             "\n"
             "    if ok and state then\n"
             "        local s = type(state) == \"table\" and (state.name or state[1] or \"table\") or tostring(state)\n"
             "        if s ~= _anim_last then\n"
             "            System.LogAlways(\"[AnimState] \" .. s)\n"
             "            _anim_last = s\n"
             "        end\n"
             "    end\n"
             "\n"
             "    Script.SetTimerForFunction(100, \"anim_poll\", nil)\n"
             "end\n"
             "\n"
             "anim_poll()\n"},
            {"Ragdoll",
             "-- Puts the closest enemy in ragdoll mode\n"
             "local pos = player:GetPos()\n"
             "local best, bestD = nil, 1e9\n"
             "for _, e in pairs(System.GetEntities(pos, 30) or {}) do\n"
             "    if e.id ~= player.id and e.soul then\n"
             "        local ep = e:GetPos()\n"
             "        local d = (ep.x-pos.x)^2 + (ep.y-pos.y)^2 + (ep.z-pos.z)^2\n"
             "        if d < bestD then best, bestD = e, d end\n"
             "    end\n"
             "end\n"
             "if not best then System.LogAlways(\"No NPC in 30m.\"); return end\n"
             "\n"
             "-- stamina damage only, reactions enabled\n"
             "best.soul:DealDamage(15, 0, player.id, false)\n"},
        };

        static void LoadLuaExample(const char* code) {
            if (!code)
                return;
            size_t len = strlen(code);
            if (len >= sizeof(s_luaBuffer))
                len = sizeof(s_luaBuffer) - 1;
            memcpy(s_luaBuffer, code, len);
            s_luaBuffer[len] = '\0';
            s_cursorBytePos = 0;
        }

        // ──────────────────────────────────────────────────────────────────────
        //  Log Viewer Helpers
        // ──────────────────────────────────────────────────────────────────────
        static int s_logSelStart = 0;
        static int s_logSelEnd = 0;
        static int s_logBufTextLen = 0;

        static int LogInputTextCallback(ImGuiInputTextCallbackData* data) {
            s_logSelStart = data->SelectionStart;
            s_logSelEnd = data->SelectionEnd;
            s_logBufTextLen = data->BufTextLen;
            return 0;
        }

        static bool SubstrExistsInsensitive(const char* start, const char* end, const char* search) {
            size_t searchLen = strlen(search);
            if ((size_t)(end - start) < searchLen)
                return false;
            for (const char* p = start; p <= end - searchLen; ++p) {
                bool match = true;
                for (size_t i = 0; i < searchLen; ++i) {
                    if (std::tolower(static_cast<unsigned char>(p[i])) != std::tolower(static_cast<unsigned char>(search[i]))) {
                        match = false;
                        break;
                    }
                }
                if (match)
                    return true;
            }
            return false;
        }

        static void RenderHighlightedLog(ImDrawList* dl, ImVec2 startPos, const char* text) {
            if (!text || !*text)
                return;

            ImFont* font = ig::GetFont( );
            const float fontSize = font->FontSize;
            const float lineHeight = ig::GetTextLineHeight( );

            float x = startPos.x;
            float y = startPos.y;

            const char* p = text;
            while (*p) {
                const char* e = strchr(p, '\n');
                if (!e)
                    e = p + strlen(p);

                ImU32 col = IM_COL32(220, 220, 220, 255);

                if (SubstrExistsInsensitive(p, e, "[error]") || SubstrExistsInsensitive(p, e, "error:")) {
                    col = IM_COL32(255, 80, 80, 255);
                } else if (SubstrExistsInsensitive(p, e, "[warn") || SubstrExistsInsensitive(p, e, "warning:")) {
                    col = IM_COL32(255, 215, 0, 255);
                } else if (SubstrExistsInsensitive(p, e, "[info") || SubstrExistsInsensitive(p, e, "info:")) {
                    col = IM_COL32(78, 201, 176, 255);
                } else if (SubstrExistsInsensitive(p, e, "[debug") || SubstrExistsInsensitive(p, e, "debug:")) {
                    col = IM_COL32(106, 153, 85, 255);
                }

                dl->AddText(font, fontSize, ImVec2(x, y), col, p, e);

                y += lineHeight;
                p = e;
                if (*p == '\n')
                    p++;
            }
        }

        static void RenderLuaPanel( ) {
            ig::TextDisabled("Type or paste multi-line Lua scripts below.");
            ig::Spacing( );

            static float s_logSectionHeight = 160.f;

            const float footerHeight = ig::GetFrameHeightWithSpacing( ) +
                                       ig::GetTextLineHeightWithSpacing( ) * 2.0f;
            const float splitterHeight = 12.f;

            float totalAvail = ig::GetContentRegionAvail( ).y;

            if (s_logSectionHeight < 50.f)
                s_logSectionHeight = 50.f;
            if (s_logSectionHeight > totalAvail - 100.f)
                s_logSectionHeight = totalAvail - 100.f;

            const float editorHeightAvail = totalAvail - footerHeight - s_logSectionHeight - splitterHeight;
            const float sidebarWidth = 145.f;

            if (ig::BeginChild("##lua_examples", ImVec2(sidebarWidth, editorHeightAvail), true)) {
                ig::TextDisabled("Examples");
                ig::Separator( );
                ig::Spacing( );
                for (const LuaExample& ex : kLuaExamples) {
                    if (ig::Button(ex.label, ImVec2(-1.f, 0.f))) {
                        LoadLuaExample(ex.code);
                        s_luaLastAction.clear( );
                    }
                    if (ig::IsItemHovered( )) {
                        ig::BeginTooltip( );
                        ig::PushTextWrapPos(380.f);
                        ig::TextUnformatted(ex.code);
                        ig::PopTextWrapPos( );
                        ig::EndTooltip( );
                    }
                }
            }
            ig::EndChild( );

            ig::SameLine( );

            const float lineHeight = ig::GetTextLineHeight( );
            const ImVec2 framePad = ig::GetStyle( ).FramePadding;

            int lineCount = 1;
            for (const char* p = s_luaBuffer; *p; ++p)
                if (*p == '\n')
                    ++lineCount;

            const float visibleHeight = editorHeightAvail;
            const float contentHeight = (float)(lineCount + 1) * lineHeight + framePad.y * 2.0f;
            const float editorHeight = (contentHeight > visibleHeight)
                                           ? contentHeight
                                           : visibleHeight;

            if (ig::BeginChild("##lua_editor_scroll", ImVec2(0.f, visibleHeight), false,
                               ImGuiWindowFlags_HorizontalScrollbar)) {
                const ImVec2 framePos = ig::GetCursorScreenPos( );
                const ImVec2 textPos = ImVec2(framePos.x + framePad.x, framePos.y + framePad.y);
                const float frameW = ig::GetContentRegionAvail( ).x;

                ImDrawList* dl = ig::GetWindowDrawList( );

                const ImU32 bgCol = ig::GetColorU32(ImGuiCol_FrameBg);
                dl->AddRectFilled(framePos,
                                  ImVec2(framePos.x + frameW, framePos.y + editorHeight),
                                  bgCol, ig::GetStyle( ).FrameRounding);

                std::vector<LuaToken> tokens;
                TokenizeLua(s_luaBuffer, tokens);
                RenderHighlightedLua(dl, textPos, s_luaBuffer, tokens);

                UpdateLuaCompletion( );

                if (s_pendingRefocus) {
                    ig::SetKeyboardFocusHere( );
                    s_pendingRefocus = false;
                }

                ig::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
                ig::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                ig::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
                ig::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));

                ig::InputTextMultiline("##lua_input", s_luaBuffer, sizeof(s_luaBuffer),
                                       ImVec2(-1.f, editorHeight),
                                       ImGuiInputTextFlags_AllowTabInput |
                                           ImGuiInputTextFlags_CallbackAlways |
                                           ImGuiInputTextFlags_CallbackCharFilter,
                                       LuaInputTextCallback);

                ig::PopStyleColor(4);

                const bool inputActive = ig::IsItemActive( );

                if (inputActive) {
                    const ImVec2 cp = LuaCursorScreenPos(textPos, s_luaBuffer,
                                                         s_cursorBytePos, lineHeight);

                    if (s_completion.available && s_completion.suffix[0]) {
                        ImFont* font = ig::GetFont( );
                        dl->AddText(font, font->FontSize, cp,
                                    IM_COL32(170, 170, 170, 170),
                                    s_completion.suffix);
                    }

                    const bool blink = ((int)(ig::GetTime( ) * 1.6) & 1) == 0;
                    if (blink) {
                        dl->AddLine(cp,
                                    ImVec2(cp.x, cp.y + lineHeight),
                                    IM_COL32(230, 230, 230, 255), 1.0f);
                    }

                    const float cursorYInChild = cp.y - ig::GetWindowPos( ).y + ig::GetScrollY( );
                    const float viewTop = ig::GetScrollY( );
                    const float viewBottom = viewTop + ig::GetWindowHeight( );
                    if (cursorYInChild < viewTop)
                        ig::SetScrollY(cursorYInChild);
                    else if (cursorYInChild + lineHeight > viewBottom)
                        ig::SetScrollY(cursorYInChild + lineHeight - ig::GetWindowHeight( ));
                } else {
                    s_completion.available = false;
                    s_completion.suffix[0] = '\0';
                }
            }
            ig::EndChild( );

            ig::Spacing( );
            ig::Separator( );
            ig::Spacing( );

            if (ig::Button("Execute", ImVec2(120.f, 0.f))) {
                if (strlen(s_luaBuffer) > 0) {
                    ExecuteLuaCode(s_luaBuffer);
                } else {
                    s_luaLastAction = "Error: Input is empty.";
                }
            }

            ig::SameLine( );

            if (ig::Button("Paste", ImVec2(80.f, 0.f))) {
                if (InsertClipboardAtCursor(s_luaBuffer, sizeof(s_luaBuffer),
                                            &s_cursorBytePos, &s_lastSelStart, &s_lastSelEnd,
                                            &s_lastBufTextLen)) {
                    s_pendingCursorPos = s_cursorBytePos;
                    s_pendingRefocus = true;
                }
            }

            ig::SameLine( );

            if (ig::Button("Copy", ImVec2(80.f, 0.f))) {
                int a = s_lastSelStart, b = s_lastSelEnd;
                if (a > b) {
                    int t = a;
                    a = b;
                    b = t;
                }
                if (a >= 0 && b > a && b <= s_lastBufTextLen) {
                    WriteClipboardUtf8(s_luaBuffer + a, (size_t)(b - a));
                } else {
                    const size_t len = strlen(s_luaBuffer);
                    if (len > 0)
                        WriteClipboardUtf8(s_luaBuffer, len);
                }
            }

            ig::SameLine( );

            if (ig::Button("Cut", ImVec2(80.f, 0.f))) {
                int a = s_lastSelStart, b = s_lastSelEnd;
                if (a > b) {
                    int t = a;
                    a = b;
                    b = t;
                }
                const size_t bufLen = strlen(s_luaBuffer);
                if (a >= 0 && b > a && (size_t)b <= bufLen) {
                    WriteClipboardUtf8(s_luaBuffer + a, (size_t)(b - a));
                    memmove(s_luaBuffer + a, s_luaBuffer + b, bufLen - b + 1);
                    s_lastSelStart = s_lastSelEnd = a;
                    s_lastBufTextLen = (int)(bufLen - (b - a));
                    s_cursorBytePos = a;
                } else if (bufLen > 0) {
                    WriteClipboardUtf8(s_luaBuffer, bufLen);
                    memset(s_luaBuffer, 0, sizeof(s_luaBuffer));
                    s_lastSelStart = s_lastSelEnd = 0;
                    s_lastBufTextLen = 0;
                    s_cursorBytePos = 0;
                }
            }

            ig::SameLine( );

            if (ig::Button("Clear", ImVec2(80.f, 0.f))) {
                memset(s_luaBuffer, 0, sizeof(s_luaBuffer));
                s_luaLastAction.clear( );
                s_cursorBytePos = 0;
                s_lastSelStart = s_lastSelEnd = 0;
                s_lastBufTextLen = 0;
            }

            if (!s_luaLastAction.empty( )) {
                ig::SameLine( );
                bool isError = (s_luaLastAction.find("Error") != std::string::npos ||
                                s_luaLastAction.find("Exception") != std::string::npos);
                ig::TextColored(isError ? ImVec4(1.f, 0.35f, 0.35f, 1.f) : ImVec4(0.55f, 0.85f, 0.55f, 1.f),
                                "  %s", s_luaLastAction.c_str( ));
            }

            ig::Spacing( );

            ig::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ig::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.4f));
            ig::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));

            ig::Button("##log_splitter", ImVec2(-1.0f, 6.0f));

            if (ig::IsItemActive( )) {
                s_logSectionHeight -= ig::GetIO( ).MouseDelta.y;
            }
            if (ig::IsItemHovered( ) || ig::IsItemActive( )) {
                ig::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }
            ig::PopStyleColor(3);

            ig::Spacing( );

            s_logReader.Update( );

            ig::TextDisabled("kcd.log Viewer");

            ig::SameLine(ig::GetWindowWidth( ) - 175.f);

            if (ig::Button("Copy##log_btn")) {
                int a = s_logSelStart, b = s_logSelEnd;
                if (a > b) {
                    int t = a;
                    a = b;
                    b = t;
                }
                if (a >= 0 && b > a && b <= s_logBufTextLen) {
                    WriteClipboardUtf8(s_logReader.buffer + a, (size_t)(b - a));
                } else {
                    const size_t len = strlen(s_logReader.buffer);
                    if (len > 0)
                        WriteClipboardUtf8(s_logReader.buffer, len);
                }
            }

            ig::SameLine( );
            ig::Checkbox("Auto-scroll", &s_logReader.autoScroll);

            if (ig::BeginChild("##log_scroll", ImVec2(-1.f, ig::GetContentRegionAvail( ).y), true, ImGuiWindowFlags_HorizontalScrollbar)) {
                int logLineCount = 1;
                for (const char* p = s_logReader.buffer; *p; ++p) {
                    if (*p == '\n')
                        ++logLineCount;
                }

                const float textHeight = logLineCount * lineHeight + framePad.y * 2.0f;
                const float availH = ig::GetContentRegionAvail( ).y;
                const float finalH = (textHeight > availH) ? textHeight : availH;
                const float finalW = (std::max)(3000.f, ig::GetContentRegionAvail( ).x);

                const ImVec2 framePos = ig::GetCursorScreenPos( );
                const ImVec2 textPos = ImVec2(framePos.x + framePad.x, framePos.y + framePad.y);

                ImDrawList* dl = ig::GetWindowDrawList( );

                dl->AddRectFilled(framePos, ImVec2(framePos.x + finalW, framePos.y + finalH), ig::GetColorU32(ImGuiCol_FrameBg));

                RenderHighlightedLog(dl, textPos, s_logReader.buffer);

                ig::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
                ig::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
                ig::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
                ig::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));

                ig::InputTextMultiline("##kcd_log_viewer", s_logReader.buffer, sizeof(s_logReader.buffer),
                                       ImVec2(finalW, finalH),
                                       ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_CallbackAlways,
                                       LogInputTextCallback);

                ig::PopStyleColor(4);

                if (s_logReader.autoScroll && s_logReader.updatedThisFrame) {
                    ig::SetScrollHereY(1.0f);
                }
            }
            ig::EndChild( );
        }

        // ══════════════════════════════════════════════════════════════════════
        //  Settings panel
        // ══════════════════════════════════════════════════════════════════════
        static void RenderSettingsPanel( ) {
            MenuSettings& S = s_settings;

            ig::TextDisabled("Changes apply immediately and persist for this session.");
            ig::Spacing( );

            // ── Colours ───────────────────────────────────────────────────────
            if (ig::CollapsingHeader("Colours")) {
                ig::Spacing( );

                ig::Text("Accent colour");
                ig::SetNextItemWidth(-1.f);
                ig::ColorEdit4("##accent", reinterpret_cast<float*>(&S.accentColor),
                               ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);

                ig::Text("Accent hovered");
                ig::SetNextItemWidth(-1.f);
                ig::ColorEdit4("##accentH", reinterpret_cast<float*>(&S.accentHovered),
                               ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);

                ig::Text("Accent active / pressed");
                ig::SetNextItemWidth(-1.f);
                ig::ColorEdit4("##accentA", reinterpret_cast<float*>(&S.accentActive),
                               ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);

                ig::Spacing( );
                ig::Separator( );
                ig::Spacing( );

                ig::Text("Window background");
                ig::SetNextItemWidth(-1.f);
                ig::ColorEdit3("##winbg", reinterpret_cast<float*>(&S.windowBg),
                               ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);

                ig::Text("Frame / input background");
                ig::SetNextItemWidth(-1.f);
                ig::ColorEdit3("##framebg", reinterpret_cast<float*>(&S.frameColor),
                               ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);

                ig::Text("Text colour");
                ig::SetNextItemWidth(-1.f);
                ig::ColorEdit3("##textcol", reinterpret_cast<float*>(&S.textColor),
                               ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar);
            }

            ig::Spacing( );

            // ── Typography ────────────────────────────────────────────────────
            if (ig::CollapsingHeader("Typography")) {
                ig::Spacing( );

                ig::Text("Font scale");
                ig::SetNextItemWidth(-1.f);
                ig::SliderFloat("##fontscale", &S.fontScale, 0.5f, 2.0f, "%.2fx");
                ig::SameLine( );
                if (ig::SmallButton("Reset##fs"))
                    S.fontScale = 1.0f;
            }

            ig::Spacing( );

            // ── Layout ────────────────────────────────────────────────────────
            if (ig::CollapsingHeader("Layout")) {
                ig::Spacing( );

                ig::Text("Item vertical spacing");
                ig::SetNextItemWidth(-1.f);
                ig::SliderFloat("##itemspy", &S.itemSpacingY, 2.0f, 12.0f, "%.0fpx");
                ig::SameLine( );
                if (ig::SmallButton("Reset##is"))
                    S.itemSpacingY = 4.0f;

                ig::Text("Window opacity");
                ig::SetNextItemWidth(-1.f);
                ig::SliderFloat("##winalpha", &S.windowAlpha, 0.40f, 1.0f, "%.2f");
                ig::SameLine( );
                if (ig::SmallButton("Reset##wa"))
                    S.windowAlpha = 1.0f;
            }

            ig::Spacing( );

            // ── Misc ──────────────────────────────────────────────────────────
            if (ig::CollapsingHeader("Misc")) {
                ig::Spacing( );
                ig::Checkbox("Show FPS overlay", &S.showFps);
                ig::TextDisabled("Displays a small FPS counter in the top-right corner.");
            }

            ig::Spacing( );
            ig::Separator( );
            ig::Spacing( );

            // ── Presets ───────────────────────────────────────────────────────
            ig::Text("Quick presets:");
            ig::Spacing( );

            if (ig::Button("Default (Blue)", ImVec2(140.f, 0.f))) {
                S.accentColor = ImVec4(0.163f, 0.273f, 0.495f, 1.00f);
                S.accentHovered = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
                S.accentActive = ImVec4(0.30f, 0.50f, 0.90f, 1.00f);
                S.windowBg = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
                S.frameColor = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
                S.textColor = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
                S.windowAlpha = 1.0f;
            }
            ig::SameLine( );
            if (ig::Button("Blood Red", ImVec2(100.f, 0.f))) {
                S.accentColor = ImVec4(0.75f, 0.10f, 0.10f, 1.00f);
                S.accentHovered = ImVec4(0.85f, 0.20f, 0.20f, 1.00f);
                S.accentActive = ImVec4(0.60f, 0.05f, 0.05f, 1.00f);
                S.windowBg = ImVec4(0.08f, 0.06f, 0.06f, 1.00f);
                S.frameColor = ImVec4(0.14f, 0.09f, 0.09f, 1.00f);
                S.textColor = ImVec4(1.00f, 0.90f, 0.90f, 1.00f);
            }
            ig::SameLine( );
            if (ig::Button("Poison Green", ImVec2(120.f, 0.f))) {
                S.accentColor = ImVec4(0.15f, 0.75f, 0.25f, 1.00f);
                S.accentHovered = ImVec4(0.25f, 0.85f, 0.35f, 1.00f);
                S.accentActive = ImVec4(0.10f, 0.60f, 0.15f, 1.00f);
                S.windowBg = ImVec4(0.06f, 0.10f, 0.06f, 1.00f);
                S.frameColor = ImVec4(0.09f, 0.14f, 0.09f, 1.00f);
                S.textColor = ImVec4(0.90f, 1.00f, 0.90f, 1.00f);
            }
            ig::SameLine( );
            if (ig::Button("Ghost (Translucent)", ImVec2(175.f, 0.f))) {
                S.windowBg = ImVec4(0.05f, 0.05f, 0.08f, 1.00f);
                S.frameColor = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
                S.accentColor = ImVec4(0.80f, 0.80f, 0.85f, 1.00f);
                S.accentHovered = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
                S.accentActive = ImVec4(0.65f, 0.65f, 0.70f, 1.00f);
                S.textColor = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
                S.windowAlpha = 0.72f;
            }

            ig::Spacing( );
            ig::Separator( );
            ig::Spacing( );

            // Save Settings Area
            if (ig::Button("Save Design Settings", ImVec2(-1.0f, 30.f))) {
                SaveSettings( );
            }
            if (!s_settingsSaveStatus.empty( )) {
                ImVec4 msgColor = (s_settingsSaveStatus.find("Error") != std::string::npos)
                                      ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                                      : ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                ig::TextColored(msgColor, "%s", s_settingsSaveStatus.c_str( ));
            }
        }

    } // anonymous namespace

    // ══════════════════════════════════════════════════════════════════════════════
    //  Public API
    // ══════════════════════════════════════════════════════════════════════════════

    void InitializeContext(HWND hwnd) {
        if (ig::GetCurrentContext( ))
            return;
        ImGui::CreateContext( );
        ImGui_ImplWin32_Init(hwnd);
        ImGuiIO& io = ImGui::GetIO( );
        io.IniFilename = io.LogFilename = nullptr;

        LoadSettings( ); // Auto-load saved menu design at startup
    }

    void Render( ) {
        drawlist = ig::GetBackgroundDrawList( );
        DrawESP_SEH( );
        WriteInfiniteValues_SEH( );
        WriteFrozenEntities_SEH( );

        if (Fly::flyEnabled) {
            SEH_TRY {
                Fly::flyLoop( );
            } SEH_EXCEPT {
            }
        }

        ImGuiIO& io = ImGui::GetIO( );

        // ── Force-sync modifier keys from Win32 ───────────────────────────────
        io.KeyCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        io.KeyShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        io.KeyAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        io.KeySuper = ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;

        // Optional FPS overlay (rendered before the main window so it sits on top)
        if (s_settings.showFps) {
            ig::SetNextWindowPos(ImVec2(io.DisplaySize.x - 80.f, 8.f), ImGuiCond_Always);
            ig::SetNextWindowSize(ImVec2(72.f, 24.f), ImGuiCond_Always);
            ig::SetNextWindowBgAlpha(0.50f);
            ig::Begin("##fps_overlay", nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                          ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
            ig::Text("%.0f FPS", io.Framerate);
            ig::End( );
        }

        if (!bShowMenu) {
            io.MouseDrawCursor = false;
            return;
        }

        static Category selected = Category::Tutorial;
        const bool ingame = (igh::localPlayer != NULL);

        io.MouseDrawCursor = true;

        PushMenuStyle( );
        ig::SetNextWindowSize(ImVec2(720.f, 420.f), ImGuiCond_FirstUseEver);
        if (ig::Begin("Nightfall's KCD2 Internal v2.0", &bShowMenu, ImGuiWindowFlags_NoCollapse)) {
            if (!ingame) {
                if (ig::BeginChild("##startup_tutorial", ImVec2(0.f, 0.f), true))
                    RenderTutorialPanel(false);
                ig::EndChild( );
            } else {
                if (ig::BeginChild("##nav", ImVec2(190.f, 0.f), true)) {
                    ig::TextDisabled("Categories");
                    ig::Spacing( );
                    if (ig::Selectable("Tutorial / Help ##1", selected == Category::Tutorial))
                        selected = Category::Tutorial;
                    if (ig::Selectable("Item Spawner ##2", selected == Category::Inventory))
                        selected = Category::Inventory;
                    if (ig::Selectable("Entities ##3", selected == Category::Entities))
                        selected = Category::Entities;
                    if (ig::Selectable("Debug Menu ##4", selected == Category::Debug))
                        selected = Category::Debug;
                    if (ig::Selectable("Lua Executor ##5", selected == Category::Lua))
                        selected = Category::Lua;
                    if (ig::Selectable("Settings ##6", selected == Category::Settings))
                        selected = Category::Settings;
                }
                ig::EndChild( );

                ig::SameLine( );

                if (ig::BeginChild("##content", ImVec2(0.f, 0.f), true)) {
                    if (selected == Category::Tutorial) {
                        RenderTutorialPanel(true);
                    } else if (selected == Category::Inventory) {
                        ig::Text("Item Spawner");
                        ig::Separator( );
                        RenderInventoryPanel( );
                    } else if (selected == Category::Entities) {
                        ig::Text("Entity Manager");
                        ig::Separator( );
                        RenderEntityPanel( );
                    } else if (selected == Category::Debug) {
                        ig::Text("Debug Menu");
                        ig::Separator( );
                        RenderDebugMenu( );
                    } else if (selected == Category::Lua) {
                        ig::Text("Lua Executor");
                        ig::Separator( );
                        RenderLuaPanel( );
                    } else if (selected == Category::Settings) {
                        ig::Text("Settings");
                        ig::Separator( );
                        RenderSettingsPanel( );
                    }
                }
                ig::EndChild( );
            }
        }
        ig::End( );
        PopMenuStyle( );
    }

} // namespace Menu
