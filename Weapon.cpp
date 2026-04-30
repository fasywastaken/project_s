//
// Created by User on 19/04/2026.
//

#include "Weapon.h"

Weapon::Weapon(SDL_GPUTexture* tex, float w, float h, float gX, float gY, float uX, float uY, float uW, float uH)
    : texture(tex), width(w), height(h), gripOffsetX(gX), gripOffsetY(gY),
      uvX(uX), uvY(uY), uvW(uW), uvH(uH), damage(10), fireRate(0.2f), isAkimbo(false) {}
