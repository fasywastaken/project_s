#include "WaveManager.h"
#include "Player.h"
#include "Enemy.h"
#include <cmath>
#include <algorithm>
#include <SDL3/SDL.h>

WaveManager::WaveManager() : currentWave(0) {}

void WaveManager::TriggerNextWave(const Player& player, Enemy* enemies, int maxEnemies) {
    currentWave++;

    // Wave Scaling Formulas
    int enemiesToSpawn = 3 + (currentWave * 2);
    int waveHP         = 100 + ((currentWave - 1) * 25);
    int waveDamage     = 20 + ((currentWave - 1) * 5);
    float waveSpeed    = std::min(340.0f, 180.0f + ((currentWave - 1) * 15.0f)); // Cap speed to prevent clipping

    SDL_Log("=== WAVE %d STARTED ===", currentWave);
    SDL_Log("Spawning %d Harder Enemies (HP: %d | DMG: %d | SPD: %.1f)",
            enemiesToSpawn, waveHP, waveDamage, waveSpeed);

    int spawnedCount = 0;
    constexpr float SPAWN_RADIUS = 750.0f; // Keeps spawns safely off-screen

    for (int i = 0; i < maxEnemies; ++i) {
        if (!enemies[i].active) {
            // Distribute spawns evenly in a circle around the player
            float angle = (static_cast<float>(spawnedCount) / static_cast<float>(enemiesToSpawn)) * 2.0f * 3.1415926f;
            float spawnX = player.x + std::cos(angle) * SPAWN_RADIUS;
            float spawnY = player.y + std::sin(angle) * SPAWN_RADIUS;

            // Initialize the enemy
            enemies[i].Spawn(spawnX, spawnY);

            // Mutate with scaled wave stats
            enemies[i].hp     = waveHP;
            enemies[i].damage = waveDamage;
            enemies[i].speed  = waveSpeed;

            spawnedCount++;

            if (spawnedCount >= enemiesToSpawn) {
                break;
            }
        }
    }

    if (spawnedCount < enemiesToSpawn) {
        SDL_Log("Warning: Max enemy pool limit reached! Spawned %d/%d enemies.", spawnedCount, enemiesToSpawn);
    }
}

void WaveManager::Reset() {
    currentWave = 0;
}