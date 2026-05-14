//
// Created by User on 19/04/2026.
//

#pragma once
#include <SDL3/SDL.h>

class Weapon {
public:
    SDL_GPUTexture* texture;
    float width, height;
    float gripOffsetX, gripOffsetY;
    float uvX, uvY, uvW, uvH;
    float damage;

    float falloffStart = 500.0f;
    float falloffEnd = 1200.0f;
    float minDamagePct = 0.4f;


    float fireRate;
    float timeSinceLastShot;

    int currentAmmo;
    int magCapacity;

    int animCols, animRows;
    int totalAnimFrames;
    float reloadTime;
    float currentReloadTimer;
    bool isReloading;

    Weapon(SDL_GPUTexture* tex, float w, float h, float gX, float gY,
               float uX, float uY, float uW, float uH,
               int cols, int rows, int frames, float rTime, float fRate, int magCap, float dmg);

    void Update(float deltaTime);
};
