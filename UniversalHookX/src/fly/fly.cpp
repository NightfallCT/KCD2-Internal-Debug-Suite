#include "fly.h"
#include <cmath>
#include <windows.h>
#include <algorithm>
#define M_PI 3.14159265358979323846f

namespace Fly
{
    void flyTowards() {
        if (hk::localPlayer == nullptr || hk::viewmatrix == nullptr) return;

        if (bBoostEnabled) {
            flySpeed = flySpeed * 2.0f;
        }
        
        float dist = flySpeed * 0.01667f;

        float dirX = hk::viewmatrix->m21;
        float dirY = hk::viewmatrix->m22;
        float dirZ = hk::viewmatrix->m23;

        float mag = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
        if (mag > 0.0001f) {
            dirX /= mag;
            dirY /= mag;
            dirZ /= mag;
        }

        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.x += dirX * dist;
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.y += dirY * dist;
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.z += dirZ * dist;

        if (bBoostEnabled) {
            flySpeed = flySpeed / 2.0f;
        }
    }

    void flyBackwards() {
        if (hk::localPlayer == nullptr || hk::viewmatrix == nullptr) return;

        float dist = flySpeed * 0.01667f;

        float dirX = hk::viewmatrix->m12;
        float dirY = hk::viewmatrix->m22;
        float dirZ = hk::viewmatrix->m32;

        float mag = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
        if (mag > 0.0001f) {
            dirX /= mag;
            dirY /= mag;
            dirZ /= mag;
        }

        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.x -= dirX * dist;
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.y -= dirY * dist;
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.z -= dirZ * dist;
    }

    void flyLeft() {
        if (hk::localPlayer == nullptr || hk::viewmatrix == nullptr) return;
        float dist = flySpeed * 0.01667f;
        float dirX = hk::viewmatrix->m10;
        float dirY = hk::viewmatrix->m20;
        float dirZ = hk::viewmatrix->m30;
        float mag = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
        if (mag > 0.0001f) {
            dirX /= mag;
            dirY /= mag;
            dirZ /= mag;
        }
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.x -= dirX * dist;
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.y -= dirY * dist;
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.z -= dirZ * dist;
    }
    void flyRight() {
        if (hk::localPlayer == nullptr || hk::viewmatrix == nullptr) return;
        float dist = flySpeed * 0.01667f;
        float dirX = hk::viewmatrix->m10;
        float dirY = hk::viewmatrix->m20;
        float dirZ = hk::viewmatrix->m30;
        float mag = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
        if (mag > 0.0001f) {
            dirX /= mag;
            dirY /= mag;
            dirZ /= mag;
        }
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.x += dirX * dist;
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.y += dirY * dist;
        hk::localPlayer->pC_Entity->pC_GameObject->pC_PhysicalProxy->pC_LivingEntity->coords.z += dirZ * dist;
    }

    void flyLoop( ) {
        if (!flyEnabled)
            return;
        if (hk::localPlayer == nullptr || hk::viewmatrix == nullptr)
            return;

        float* m = reinterpret_cast<float*>(hk::viewmatrix);

        float rightX = m[0], rightY = m[1], rightZ = m[2];
        float upX = m[4], upY = m[5], upZ = m[6];
        float fwdX = m[8], fwdY = m[9], fwdZ = m[10];

        auto normalize = [](float& x, float& y, float& z) {
            float mag = std::sqrt(x * x + y * y + z * z);
            if (mag > 0.0001f) {
                x /= mag;
                y /= mag;
                z /= mag;
            }
        };
        normalize(rightX, rightY, rightZ);
        normalize(upX, upY, upZ);
        normalize(fwdX, fwdY, fwdZ);

        bool w = GetAsyncKeyState(0x57);
        bool s = GetAsyncKeyState(0x53);
        bool a = GetAsyncKeyState(0x41);
        bool d = GetAsyncKeyState(0x44);
        bool up = GetAsyncKeyState(VK_SPACE);
        bool down = GetAsyncKeyState(VK_CONTROL);
        bool shift = GetAsyncKeyState(VK_SHIFT);

        if (shift) {
            bBoostEnabled = true;
        } else {
            bBoostEnabled = false;
        }

        float moveX = 0.f, moveY = 0.f, moveZ = 0.f;

        if (w) {
            moveX += fwdX;
            moveY += fwdY;
            moveZ += fwdZ;
        }
        if (s) {
            moveX -= fwdX;
            moveY -= fwdY;
            moveZ -= fwdZ;
        }
        if (d) {
            moveX += rightX;
            moveY += rightY;
            moveZ += rightZ;
        }
        if (a) {
            moveX -= rightX;
            moveY -= rightY;
            moveZ -= rightZ;
        }
        if (up) {
            moveX += upX;
            moveY += upY;
            moveZ += upZ;
        }
        if (down) {
            moveX -= upX;
            moveY -= upY;
            moveZ -= upZ;
        }

        float moveMag = std::sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
        if (moveMag < 0.0001f)
            return;

        moveX /= moveMag;
        moveY /= moveMag;
        moveZ /= moveMag;

        float speed = bBoostEnabled ? flySpeed * 2.0f : flySpeed;
        float dist = speed * 0.01667f;

        auto& coords = hk::localPlayer->pC_Entity->pC_GameObject
                           ->pC_PhysicalProxy->pC_LivingEntity->coords;
        coords.x += moveX * dist;
        coords.y += moveY * dist;
        coords.z += moveZ * dist;
    }
}
