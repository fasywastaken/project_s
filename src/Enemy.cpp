//
// Created by User on 15/05/2026.
//

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
    attackCooldown = 1.2f;
    currentAttackTimer = 0.0f;

    currentFrame = 0; // 0 = Idle
    frameTimer = 0.0f;
    isAttacking = false;

    timeSinceLastStep = 0.0f;
    stepToggle = 0;
    justStepped = false;
}

void Enemy::Update(float deltaTime, const Player& player) {
    if (!active) return;

    float dx = player.x - x;
    float dy = player.y - y;
    float distance = std::hypot(dx, dy);

    // 1. Handle Attack Cooldown
    if (currentAttackTimer > 0.0f) {
        currentAttackTimer -= deltaTime;
    }

    // 2. Pathfinding & State Selection
    if (distance <= attackRange) {
        if (!isAttacking && currentAttackTimer <= 0.0f) {
            isAttacking = true;
            currentFrame = 9; // Jump to the first frame of the attack animation
            frameTimer = 0.0f;
        }
    } else if (!isAttacking) {
        // Move towards player
        float dirX = dx / distance;
        float dirY = dy / distance;
        x += dirX * speed * deltaTime;
        y += dirY * speed * deltaTime;

        // Force a step sound every 0.35 seconds of movement
        timeSinceLastStep += deltaTime;
        if (timeSinceLastStep > 0.35f) {
            justStepped = true;
            stepToggle = 1 - stepToggle; // Flips between 0 and 1
            timeSinceLastStep = 0.0f;
        }
    }

    // 3. The 4x3 Sprite Sheet Animator
    frameTimer += deltaTime;
    if (frameTimer >= 0.075f) { // Animation speed
        frameTimer = 0.0f;

        if (isAttacking) {
            currentFrame++;
            if (currentFrame > 11) {
                // Attack animation finished!
                isAttacking = false;
                currentFrame = 0;
                currentAttackTimer = attackCooldown;

                // Placeholder until we implement player health
                SDL_Log("ENEMY HIT PLAYER FOR %d DAMAGE!", damage);
            }
        } else if (distance > attackRange) {
            // Walking loop (Frames 1 through 8)
            if (currentFrame < 1 || currentFrame >= 8) {
                currentFrame = 1;
            } else {
                currentFrame++;
            }
        } else {
            // Standing close but on cooldown
            currentFrame = 0;
        }
    }
}