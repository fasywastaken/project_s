//
// Created by User on 19/04/2026.
//

#pragma once
#include <SDL3/SDL.h>

class Weapon {
public:
    SDL_GPUTexture* texture;

    // Rendering dimensions
    float width;
    float height;

    // Where the hand grips the weapon relative to its center
    float gripOffsetX;
    float gripOffsetY;

    // Spritesheet slice coordinates (0.0 to 1.0)
    float uvX, uvY, uvW, uvH;

    // Gameplay stats
    int damage;
    float fireRate;

    bool isAkimbo;

    Weapon(SDL_GPUTexture* tex, float w, float h, float gX, float gY, float uX, float uY, float uW, float uH);
};
