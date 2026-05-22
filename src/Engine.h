// created by fasy on 19/04/2026

#pragma once
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include "Enemy.h"
#include "WaveManager.h"
#include <vector>

class Player;
struct Vertex {
    float x, y;
    float u, v;
};

class Destructible {
public:
    virtual ~Destructible() = default;
    virtual void TakeDamage(int damage) = 0;
    float x{}, y{};
    int hp{};
    bool active = true;
};

struct Tree : public Destructible {
    int variant;
    float scale = 1.0f;

    Tree(float x, float y, int var, bool act, int h, float s)
        : variant(var), scale(s) {
        this->x = x;
        this->y = y;
        this->active = act;
        this->hp = h;
    }

    void TakeDamage(int damage) override {
        hp -= damage;
        scale = std::max(0.3f, static_cast<float>(hp) / 50.0f);
        if (hp <= 0) active = false;
    }
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
    friend class Player;

    // core state
    bool isRunning;
    SDL_Window* window;
    float screenWidth{};
    float screenHeight{};
    SDL_GPUDevice* gpuDevice;
    WaveManager waveManager;

    // graphics pipeline and memory
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* unitQuad;

    SDL_GPUTexture* waterTex;
    SDL_GPUTexture* sandTex;
    SDL_GPUTexture* grassTex;

    SDL_GPUTexture* treeTex;
    std::vector<Tree> trees;

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

    void GenerateMap(int width, int height);
    static int GetTileValue(int x, int y, const int* grid, int w, int h);

    static void ApplyBlur(int* grid, int w, int h, std::vector<int>& buffer);

    // game logic state
    float playerX, playerY;
    int playerWidth, playerHeight;
    int* mapGrid;
    int mapWidth, mapHeight;
    bool DamageDestructibles(float hitX, float hitY, int damage, float radius);

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