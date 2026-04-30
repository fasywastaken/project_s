// created by fasy on 19/04/2026

#include "Player.h"
#include <cmath>

Player::Player(float startX, float startY)
    : x(startX), y(startY), speed(400.0f), armAngle(0.0f),
      isAttacking(false), currentFrame(0), frameTimer(0.0f),
      currentCombo(1), equippedWeapon(nullptr) {
    inventory[0] = nullptr;
    inventory[1] = nullptr;
}

void Player::Update(float deltaTime, const bool* keys, float mouseX, float mouseY, const int* mapGrid, int mapWidth, int mapHeight) {
    // calculate raw input direction
    float inputX = 0.0f;
    float inputY = 0.0f;

    if (keys[SDL_SCANCODE_W]) inputY -= 1.0f;
    if (keys[SDL_SCANCODE_S]) inputY += 1.0f;
    if (keys[SDL_SCANCODE_A]) inputX -= 1.0f;
    if (keys[SDL_SCANCODE_D]) inputX += 1.0f;

    if (inputX != 0.0f && inputY != 0.0f) {
        inputX *= 0.707106f;
        inputY *= 0.707106f;
    }

    int centerGridX = static_cast<int>((x + 16.0f) / 32.0f);
    int centerGridY = static_cast<int>((y + 16.0f) / 32.0f);

    int standingOnTile = 0;
    if (centerGridX >= 0 && centerGridX < mapWidth && centerGridY >= 0 && centerGridY < mapHeight) {
        standingOnTile = mapGrid[centerGridY * mapWidth + centerGridX];
    }

    float speedMultiplier = 1.0f;

    // water slow
    if (standingOnTile == 1) {
        speedMultiplier = 0.66f;
    }

    float dx = inputX * speed * speedMultiplier * deltaTime;
    float dy = inputY * speed * speedMultiplier * deltaTime;

    justStepped = false;

    if (stepCooldown > 0.0f) {
        stepCooldown -= deltaTime;
    }

    if (dx != 0.0f || dy != 0.0f) {

        if (stepCooldown <= 0.0f) {
            justStepped = true;
            stepToggle = 1 - stepToggle;
            currentTile = standingOnTile;

            stepCooldown = (standingOnTile == 1) ? 0.60f : 0.40f;
        }
    }


    // physics and collision system
    float hitboxRadius = 24.0f;
    float tileSize = 32.0f;

    // x-axis collision
    if (dx != 0.0f) {
        float proposedX = x + dx;
        float checkX = (dx > 0) ? proposedX + hitboxRadius : proposedX - hitboxRadius;

        int gridX = static_cast<int>(checkX / tileSize);
        int gridYTop = static_cast<int>((y - hitboxRadius) / tileSize);
        int gridYBot = static_cast<int>((y + hitboxRadius) / tileSize);

        if (gridX >= 0 && gridX < mapWidth && gridYTop >= 0 && gridYBot < mapHeight) {
            if (mapGrid[gridYTop * mapWidth + gridX] != 4 &&
                mapGrid[gridYBot * mapWidth + gridX] != 4) {
                x = proposedX;
            }
        }
    }

    // y-axis collision
    if (dy != 0.0f) {
        float proposedY = y + dy;
        float checkY = (dy > 0) ? proposedY + hitboxRadius : proposedY - hitboxRadius;

        int gridY = static_cast<int>(checkY / tileSize);
        int gridXLeft = static_cast<int>((x - hitboxRadius) / tileSize);
        int gridXRight = static_cast<int>((x + hitboxRadius) / tileSize);

        if (gridY >= 0 && gridY < mapHeight && gridXLeft >= 0 && gridXRight < mapWidth) {
            if (mapGrid[gridY * mapWidth + gridXLeft] != 4 &&
                mapGrid[gridY * mapWidth + gridXRight] != 4) {
                y = proposedY;
            }
        }
    }

    // aiming
    float aimDx = mouseX - x;
    float aimDy = mouseY - y;
    armAngle = std::atan2(aimDy, aimDx);

    // universal 3-frame attack loop
    if (isAttacking) {
        frameTimer += deltaTime;
        if (frameTimer > 0.08f) {
            frameTimer = 0.0f;
            currentFrame++;
            if (currentFrame > 3) {
                isAttacking = false;
                currentFrame = 0;
            }
        }
    } else {
        currentFrame = 0;
    }
}

void Player::Attack() {
    if (!isAttacking) {
        isAttacking = true;
        currentCombo = (currentCombo == 1) ? 2 : 1;

        currentFrame = 1;
        frameTimer = 0.0f;
    }
}

void Player::SetInventorySlot(int slot, Weapon* weapon) {
    if (slot >= 0 && slot < 2) {
        inventory[slot] = weapon;
    }
}

void Player::EquipSlot(int slot) {
    if (slot == 0 || slot == 1) {
        if (inventory[slot] != nullptr) {
            equippedWeapon = inventory[slot];
        }
    } else if (slot == 2) {
        equippedWeapon = nullptr;
    }
}