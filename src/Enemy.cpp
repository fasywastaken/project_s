//
// Created by User on 15/05/2026.
//

#include "Engine.h"
#include "Enemy.h"
#include "Player.h"
#include <cmath>
#include <SDL3/SDL.h>

Enemy::Enemy() : active(false), x(0), y(0), speed(0), hp(0), damage(0), attackRange(0), attackCooldown(0),
                 currentAttackTimer(0),
                 currentFrame(0),
                 frameTimer(0),
                 isAttacking(false),
                 timeSinceLastStep(0), stepToggle(0),
                 justStepped(false) {
}

void Enemy::Spawn(float startX, float startY) {
    active = true;
    x = startX;
    y = startY;

    speed = 180.0f;
    hp = 100;
    damage = 20;

    attackRange = 45.0f;
    attackCooldown = 0.5f;
    currentAttackTimer = 0.0f;

    currentFrame = 0; // 0 = Idle
    frameTimer = 0.0f;
    isAttacking = false;

    timeSinceLastStep = 0.0f;
    stepToggle = 0;
    justStepped = false;
}

void Enemy::Update(float deltaTime, const Player& player, const Enemy* allEnemies, int maxEnemies, const std::vector<Tree>& trees) {
    if (!active) return;

    float dx = player.x - x;
    float dy = player.y - y;
    float distToPlayer = std::hypot(dx, dy);

    if (currentAttackTimer > 0.0f) {
        currentAttackTimer -= deltaTime;
    }

    float pushRadius = 64.0f;
    float sepX = 0.0f;
    float sepY = 0.0f;

    if (allEnemies != nullptr) {
        for (int j = 0; j < maxEnemies; ++j) {
            if (!allEnemies[j].active || &allEnemies[j] == this) continue;

            float neighborDx = x - allEnemies[j].x;
            float neighborDy = y - allEnemies[j].y;
            float distToNeighbor = std::hypot(neighborDx, neighborDy);

            if (distToNeighbor > 0.0f && distToNeighbor < pushRadius) {
                float pushStrength = 1.0f - (distToNeighbor / pushRadius);
                sepX += (neighborDx / distToNeighbor) * pushStrength;
                sepY += (neighborDy / distToNeighbor) * pushStrength;
            }
        }
    }

    if (distToPlayer <= attackRange) {
        if (!isAttacking && currentAttackTimer <= 0.0f) {
            isAttacking = true;
            currentFrame = 8;
            frameTimer = 0.0f;
        }
    } else if (!isAttacking) {
        float pushForce = 240.0f;
        float dirX = dx / distToPlayer;
        float dirY = dy / distToPlayer;

        float finalVx = (dirX * speed) + (sepX * pushForce);
        float finalVy = (dirY * speed) + (sepY * pushForce);

        x += finalVx * deltaTime;
        y += finalVy * deltaTime;

        timeSinceLastStep += deltaTime;
        if (timeSinceLastStep > 0.35f) {
            justStepped = true;
            stepToggle = 1 - stepToggle;
            timeSinceLastStep = 0.0f;
        }
    }

    for (const auto& tree : trees) {
        constexpr float enemyRadius = 32.0f;
        constexpr float stemRadius  = 21.0f;
        if (!tree.active) continue;

        float treeCenterX = tree.x + 64.0f;
        float treeCenterY = tree.y + 64.0f;

        float toEnemyX = x - treeCenterX;
        float toEnemyY = y - treeCenterY;
        float dist = std::hypot(toEnemyX, toEnemyY);
        float minDist = enemyRadius + stemRadius;

        if (dist < minDist && dist > 0.0f) {
            float pushX = (toEnemyX / dist) * (minDist - dist);
            float pushY = (toEnemyY / dist) * (minDist - dist);
            x += pushX;
            y += pushY;
        }
    }

    frameTimer += deltaTime;
    if (frameTimer >= 0.075f) {
        frameTimer = 0.0f;

        if (isAttacking) {
            currentFrame++;
            if (currentFrame > 11) {
                isAttacking = false;
                currentFrame = 0;
                currentAttackTimer = attackCooldown;

                SDL_Log("ENEMY HIT PLAYER FOR %d DAMAGE!", damage);
            }
        } else if (distToPlayer > attackRange) {
            if (currentFrame < 1 || currentFrame >= 8) {
                currentFrame = 1;
            } else {
                currentFrame++;
            }
        } else {
            currentFrame = 0;
        }
    }
}