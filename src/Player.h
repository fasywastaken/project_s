//
// Created by User on 19/04/2026.

#pragma once
#include "../Weapon.h"
#include <array>

class Player {
public:
    float x, y;
    float speed = 400.0f;
    float armAngle = 0.0f;

    int armorLevel = 0;
    int activeGearLayout = 0;
    int activeSkin = 0;

    bool isAttacking = false;
    int currentFrame = 0;
    float frameTimer = 0.0f;
    int currentCombo = 1;

    float stepCooldown = 0.0f;
    int stepToggle = 0;
    bool justStepped = false;
    int currentTile = 0;

    std::array<Weapon*, 2> inventory{};
    Weapon* equippedWeapon = nullptr;

    Player(float startX, float startY);

    void Update(float deltaTime, const bool* keys, float mouseX, float mouseY,
                const int* mapGrid, int mapWidth, int mapHeight);
    void Attack();

    [[nodiscard]] bool AttemptFire(float& outGunTipX, float& outGunTipY);
    void TriggerReload();

    void SetInventorySlot(int slot, Weapon* weapon);
    void EquipSlot(int slot);
};
