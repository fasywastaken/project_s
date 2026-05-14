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

    if (currentAttackTimer > 0.0f) {
        currentAttackTimer -= deltaTime;
    }

    if (distance <= attackRange) {
        if (!isAttacking && currentAttackTimer <= 0.0f) {
            isAttacking = true;
            currentFrame = 9;
            frameTimer = 0.0f;
        }
    } else if (!isAttacking) {
        float dirX = dx / distance;
        float dirY = dy / distance;
        x += dirX * speed * deltaTime;
        y += dirY * speed * deltaTime;

        timeSinceLastStep += deltaTime;
        if (timeSinceLastStep > 0.35f) {
            justStepped = true;
            stepToggle = 1 - stepToggle;
            timeSinceLastStep = 0.0f;
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
        } else if (distance > attackRange) {
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