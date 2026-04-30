//
// Created by User on 19/04/2026.
//

#pragma once
#include "Weapon.h"

class Player {
public:
    float x, y;
    float speed;
    float armAngle;

    bool isAttacking;
    int currentFrame;
    float frameTimer;
    int currentCombo; // Tracks 1 or 2 for alternating hands

    float stepCooldown = 0.0f;
    int stepToggle = 0;
    bool justStepped = false;
    int currentTile = 0;

    Weapon* inventory[2]{};
    Weapon* equippedWeapon;

    Player(float startX, float startY);

    void Update(float deltaTime, const bool* keys, float mouseX, float mouseY, const int* mapGrid, int mapWidth, int mapHeight);
    void Attack(); // Cleaned up arguments

    void SetInventorySlot(int slot, Weapon* weapon);
    void EquipSlot(int slot);
};
