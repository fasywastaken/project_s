// created by fasy on 19/04/2026

#pragma once
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
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

    float armorColor[4];
    float padding[4];
};

struct Tracer {
    float startX, startY;
    float currentX, currentY;
    float angle;
    float speed;
    float length;
    float life;
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
    SDL_GPUDevice* gpuDevice;

    // graphics pipeline and memory
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* unitQuad;

    SDL_GPUTexture* waterTex;
    SDL_GPUTexture* sandTex;
    SDL_GPUTexture* grassTex;

    SDL_GPUTexture* activePaletteTex{};
    SDL_GPUTexture* testPaletteTex{};
    SDL_GPUSampler* paletteSamplerObj;

    SDL_GPUTexture* playerTex;
    SDL_GPUSampler* playerSampler;
    SDL_GPUTexture* crosshairTex;
    SDL_GPUTexture* armTex;
    SDL_GPUTexture* ak74Tex;

    // audio logic
    MIX_Mixer* mainMixer = nullptr;
    MIX_Track* footstepTracks[2] = {nullptr, nullptr};

    MIX_Audio* waterSteps[2];
    MIX_Audio* sandSteps[2];
    MIX_Audio* grassSteps[2];

    // game logic state
    float playerX, playerY;
    int playerWidth, playerHeight;
    int* mapGrid;
    int mapWidth, mapHeight;

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
                  SDL_GPUTexture* spriteTex, SDL_GPUTexture* paletteTex,
                  const PushConstants& pushData) const;

    std::vector<Tracer> activeTracers;
    SDL_GPUTexture* tracer762Tex;
};