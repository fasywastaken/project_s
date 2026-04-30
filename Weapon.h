//
// Created by User on 19/04/2026.
//

#pragma once
#include <SDL3/SDL.h>

class Weapon {
public:
    SDL_GPUTexture* texture;

    float width;
    float height;

    float gripOffsetX;
    float gripOffsetY;

    float uvX, uvY, uvW, uvH;

    int damage;
    float fireRate;

    bool isAkimbo;

    Weapon(SDL_GPUTexture* tex, float w, float h, float gX, float gY, float uX, float uY, float uW, float uH);
};
