//
// Created by User on 15/05/2026.
//

#pragma once
#include <vector>
struct Tree;

class Player;

class Enemy {
public:
    bool active;

    float x, y;
    float speed;
    int hp;
    int damage;

    float attackRange;
    float attackCooldown;
    float currentAttackTimer;

    int currentFrame;
    float frameTimer;
    bool isAttacking;

    float timeSinceLastStep;
    int stepToggle;
    bool justStepped;

    Enemy();

    void Spawn(float startX, float startY);
    void Update(float deltaTime, const Player& player, const Enemy* allEnemies, int maxEnemies, const std::vector<Tree>& trees);
};
