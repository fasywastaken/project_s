//
// Created by User on 19/04/2026.
//

#include "../Weapon.h"

Weapon::Weapon(SDL_GPUTexture* tex, float w, float h, float gX, float gY,
               float uX, float uY, float uW, float uH,
               int cols, int rows, int frames, float rTime, float fRate, int magCap)
    : texture(tex), width(w), height(h), gripOffsetX(gX), gripOffsetY(gY),
      uvX(uX), uvY(uY), uvW(uW), uvH(uH), damage(10),
      fireRate(fRate), timeSinceLastShot(0.0f),
      currentAmmo(magCap), magCapacity(magCap),
      animCols(cols), animRows(rows), totalAnimFrames(frames),
      reloadTime(rTime), currentReloadTimer(0.0f), isReloading(false) {}

void Weapon::Update(float deltaTime) {
    timeSinceLastShot += deltaTime;

    if (isReloading) {
        currentReloadTimer += deltaTime;
        if (currentReloadTimer >= reloadTime) {
            currentAmmo = magCapacity;
            isReloading = false;
            currentReloadTimer = 0.0f;
        }
    }
}