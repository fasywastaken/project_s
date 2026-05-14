// created by fasy on 19/04/2026

#include "Player.h"
#include <cmath>
#include <algorithm>

Player::Player(float startX, float startY)
    : x(startX), y(startY), speed(400.0f), armAngle(0.0f),
      isAttacking(false), currentFrame(0), frameTimer(0.0f),
      currentCombo(1), equippedWeapon(nullptr), distanceWalked(0.0f),
      timeSinceLastStep(0.0f), wasMoving(false){
    inventory[0] = nullptr;
    inventory[1] = nullptr;
}

static constexpr float TILE_SIZE = 32.0f;
[[maybe_unused]] static constexpr float HALF_TILE = TILE_SIZE * 0.5f;
[[maybe_unused]] static constexpr float DIAGONAL_SCALE = 1.0f / std::numbers::sqrt2_v<float>;
static constexpr float GUN_TIP_OFFSET = 65.0f;

void Player::Update(float deltaTime, const bool* keys, float mouseX, float mouseY, const int* mapGrid, int mapWidth, int mapHeight) {
    // aimimg
    float aimDx = mouseX - x;
    float aimDy = mouseY - y;
    float aimDistance = std::hypot(aimDx, aimDy);

    // the deadzone
    if (aimDistance > 1.0f) {
        armAngle = std::atan2(aimDy, aimDx);
    }

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

    inputX = std::clamp(inputX, -1.0f, 1.0f);
    inputY = std::clamp(inputY, -1.0f, 1.0f);

    int centerGridX = static_cast<int>((x + HALF_TILE) / TILE_SIZE);
    int centerGridY = static_cast<int>((y + HALF_TILE) / TILE_SIZE);

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

    // physics and collision system
    float hitboxRadius = 24.0f;
    float tileSize = 32.0f;

    // ADD THIS: Save the starting position
    float oldX = x;
    float oldY = y;

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

    // THE HYBRID FOOTSTEP SYSTEM
    float actualDistMoved = std::hypot(x - oldX, y - oldY);
    bool isMoving = (inputX != 0.0f || inputY != 0.0f);

    timeSinceLastStep += deltaTime;

    if (isMoving) {
        if (!wasMoving && timeSinceLastStep > 0.25f) {
            justStepped = true;
            stepToggle = 1 - stepToggle;
            currentTile = standingOnTile;

            distanceWalked = 0.0f;
            timeSinceLastStep = 0.0f;
        }
        else {
            distanceWalked += actualDistMoved;
            float stepDistanceThreshold = 145.0f;

            if (distanceWalked >= stepDistanceThreshold) {
                justStepped = true;
                stepToggle = 1 - stepToggle;
                currentTile = standingOnTile;
                distanceWalked = 0.0f;
                timeSinceLastStep = 0.0f;
            }
        }
    } else {
        if (distanceWalked > 0.01f) {
            distanceWalked -= 250.0f * deltaTime;
            if (distanceWalked < 0.0f) distanceWalked = 0.0f;
        }
    }

    wasMoving = isMoving;
    if (equippedWeapon != nullptr) {
        equippedWeapon->Update(deltaTime);
    }

    if (isAttacking) {
        Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
        bool holdingFire = (mouseState & SDL_BUTTON_LMASK) &&
                           equippedWeapon != nullptr &&
                           !equippedWeapon->isReloading &&
                           equippedWeapon->currentAmmo > 0;

        if (holdingFire) {
            currentFrame = 1;
            frameTimer = 0.0f;
        } else {
            frameTimer += deltaTime;
            if (frameTimer > 0.08f) {
                frameTimer = 0.0f;
                currentFrame++;

                if (currentFrame >= 4) {
                    isAttacking = false;
                    currentFrame = 0;
                }
            }
        }
    } else {
        currentFrame = 0;
    }
}


void Player::Melee() {
    if (!isAttacking) {
        isAttacking = true;
        currentCombo = (currentCombo == 1) ? 2 : 1;

        currentFrame = 1;
        frameTimer = 0.0f;

        if (equippedWeapon == nullptr) justSwung = true;
    }
}

bool Player::AttemptFire(float& outGunTipX, float& outGunTipY) {
    if (equippedWeapon == nullptr) {
        return false;
    }
    if (equippedWeapon->isReloading) {
        return false;
    }

    if (equippedWeapon->currentAmmo <= 0) {
        TriggerReload();
        return false;
    }
    if (equippedWeapon->timeSinceLastShot < equippedWeapon->fireRate) {
        return false;
    }
    equippedWeapon->timeSinceLastShot = 0.0f;
    equippedWeapon->currentAmmo--;

    float effectiveLength = 95.0f + equippedWeapon->gripOffsetX;
    outGunTipX = x + std::cos(armAngle) * effectiveLength;
    outGunTipY = y + std::sin(armAngle) * effectiveLength;
    justFired = true;

    return true;
}

void Player::TriggerReload() {
    if (equippedWeapon && !equippedWeapon->isReloading &&
            equippedWeapon->currentAmmo < equippedWeapon->magCapacity) {

        equippedWeapon->isReloading = true;
        equippedWeapon->currentReloadTimer = 0.0f;

        justReloaded = true;
            }
}

void Player::SetInventorySlot(int slot, Weapon* weapon) {
    if (slot >= 0 && slot < static_cast<int>(inventory.size())) {
        inventory[slot] = weapon;
    }
}

void Player::EquipSlot(int slot) {
    if (equippedWeapon != nullptr && equippedWeapon->isReloading) {
        equippedWeapon->isReloading = false;
        equippedWeapon->currentReloadTimer = 0.0f;
    }

    if (slot == 0 || slot == 1) {
        if (inventory[slot] != nullptr && equippedWeapon != inventory[slot]) {
            justEquipped = true;
        }
        equippedWeapon = (slot >= 0 && slot < static_cast<int>(inventory.size()))? inventory[slot] : nullptr;
    } else if (slot == 2) {
        equippedWeapon = nullptr;
    }
}