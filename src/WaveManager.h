#pragma once

// Forward declarations to keep compilation light
class Player;
class Enemy;

class WaveManager {
public:
    WaveManager();

    // Core functionality
    void TriggerNextWave(const Player& player, Enemy* enemies, int maxEnemies);
    void Reset();

    // Getters for UI or debug overlays
    int GetCurrentWave() const { return currentWave; }

private:
    int currentWave;
};