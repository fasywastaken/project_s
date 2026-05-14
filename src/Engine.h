// created by fasy on 19/04/2026

#pragma once
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include "Enemy.h"
#include <vector>

class Player;

struct Vertex {
    float x, y;
    float u, v;
};

struct alignas(16) PushConstants {
    float screenWidth, screenHeight;
    float posX, posY;

    float scaleX, scaleY;
    float rotCos, rotSin;

    float uvX, uvY;
    float uvW, uvH;

    float tintColor[4];
    float padding[4];
};

struct Tracer {
    float startX, startY;
    float currentX, currentY;
    float angle;
    float speed;
    float length;
    float life;

    int damage;
    float falloffStart;
    float falloffEnd;
    float minDamagePct;
};

class Engine {
public:
    Engine();
    ~Engine();

    bool Initialize();
    void Run();
    void Shutdown();

private:
    // core state
    bool isRunning;
    SDL_Window* window;
    float screenWidth{};
    float screenHeight{};
    SDL_GPUDevice* gpuDevice;

    // graphics pipeline and memory
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* unitQuad;

    SDL_GPUTexture* waterTex;
    SDL_GPUTexture* sandTex;
    SDL_GPUTexture* grassTex;

    SDL_GPUTexture* activePaletteTex{};
    SDL_GPUTexture* testPaletteTex{};
    SDL_GPUSampler* paletteSampler;
    SDL_GPUSampler* linearSampler;

    SDL_GPUTexture* playerTex;
    SDL_GPUSampler* nearestSampler;
    SDL_GPUTexture* crosshairTex;
    SDL_GPUTexture* armTex;
    SDL_GPUTexture* ak74Tex;

    // audio logic
    MIX_Mixer* mainMixer = nullptr;
    MIX_Track* footstepTracks[2] = {nullptr, nullptr};

    MIX_Audio* waterSteps[2];
    MIX_Audio* sandSteps[2];
    MIX_Audio* grassSteps[2];

    MIX_Track* weaponFireTrack;
    MIX_Track* weaponMechTrack;
    MIX_Track* playerTrack;

    MIX_Audio* punch_swing;

    MIX_Audio* ak47_switch;
    MIX_Audio* ak47_fire;
    MIX_Audio* ak47_reload;

    // game logic state
    float playerX, playerY;
    int playerWidth, playerHeight;
    int* mapGrid;
    int mapWidth, mapHeight;

    // enemy logic
    static constexpr int MAX_ENEMIES = 100;
    Enemy enemies[MAX_ENEMIES];

    // Enemy Assets
    static constexpr int MAX_ENEMY_TRACKS = 4;
    MIX_Track* enemyTracks[MAX_ENEMY_TRACKS];
    int currentEnemyTrackIndex = 0;
    float globalEnemyStepCooldown = 0.0f;

    SDL_GPUTexture* spiderTex;
    MIX_Audio* spiderSteps[2];

    // private subroutines
    bool SetupPipeline();
    SDL_GPUShader* LoadShader(const char* filepath, SDL_GPUShaderStage stage) const;
    [[nodiscard]] SDL_GPUBuffer* CreateUnitQuad() const;

    // native svg loader
    SDL_GPUTexture* LoadSVGToGPU(const char* filepath, float scale, int* outWidth, int* outHeight) const;

    // game loop helpers
    void ProcessInput(Player& player);
    void Render(const Player& player, float mouseX, float mouseY) const;
    void DrawQuad(SDL_GPUCommandBuffer* cmdBuf, SDL_GPURenderPass* renderPass,
              SDL_GPUTexture* texture, SDL_GPUTexture* palette,
              const PushConstants& pc, SDL_GPUSampler* sampler = nullptr) const;

    std::vector<Tracer> activeTracers;
    SDL_GPUTexture* tracer762Tex;
};