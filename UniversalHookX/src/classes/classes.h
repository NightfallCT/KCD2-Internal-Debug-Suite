#pragma once
#include "Windows.h"

struct vec3_t {
    float x, y, z;
};

struct vec2 {
    float x, y;
};

struct vec4 {
    float x, y, z, w;
};

struct Viewmatrix {
    float m00;float m01;float m02;float m03;
    float m10;float m11;float m12;float m13;
    float m20;float m21;float m22;float m23;
    float m30;float m31;float m32;float m33;
};

class ItemObject {
public:
    char _0[0x50]; // 0x00
    BYTE itemIDPart1[0x10]; // 0x50
    int itemIDPart2; // 0x60
    int itemAmount; // 0x64
    float durability; // 0x68
    char _1[0x6C]; // 0x6C
    DWORD64* pItemCollection1; // 0xD8
    DWORD64* pItemCollection2; // 0xE0
    DWORD64* pItemCollection3; // 0xE8
    DWORD64* pItemCollection4; // 0xF0
};

// Generated using ReClassEx

class NameClass {
public:
    char entityName[12];   // 0xC205D9D0
    char pad_0x000C[0x14]; // 0x000C

}; // Size=0x0020

class FactionClass {
public:
    char entityFaction[32]; // 0xC205D9D0

}; // Size=0x0020

class C_LivingEntity {
public:
    char pad_0x0000[0xA0];  // 0x0000
    vec3_t coords;         // 0x00A0
    char pad_0x00AC[0x35C]; // 0x00AC

}; // Size=0x0408

class C_PhysicalProxy {
public:
    char pad_0x0000[0x38];           // 0x0000
    C_LivingEntity* pC_LivingEntity; // 0x0038
    char pad_0x0040[0x3C8];          // 0x0040

}; // Size=0x0408

class C_GameObject {
public:
    char pad_0x0000[0x90];             // 0x0000
    C_PhysicalProxy* pC_PhysicalProxy; // 0x0090
    char pad_0x0098[0x370];            // 0x0098

}; // Size=0x0408

class C_Entity {
public:
    char pad_0x0000[0xB8];       // 0x0000
    C_GameObject* pC_GameObject; // 0x00B8
    char pad_0x00C0[0x748];      // 0x00C0

}; // Size=0x0808

class C_Soul {
public:
    char pad_0x0000[0x360];      // 0x0000
    NameClass* pNameClass;       // 0x0360
    char pad_0x0368[0x30];       // 0x0368
    FactionClass* pFactionClass; // 0x0398
    char pad_0x03A0[0x368];      // 0x03A0
    float health;                // 0x0708
    float stamina;               // 0x070C
    float energy;                // 0x0710
    float fullness;              // 0x0714
    char pad_0x0718[0x414];      // 0x0718
    float velocity;              // 0x0B2C
    char pad_0x0B30[0x1B0];      // 0x0B30
    C_Entity* pC_Entity;         // 0x0CE0
    char pad_0x0CE8[0x358];      // 0x0CE8

}; // Size=0x1040
