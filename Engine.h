//
// Created by Fasy on 19/04/2026.
//

#pragma once
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

class Player; // Forward declaration

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

    float padding[4];
};

class Engine {
public:
    Engine();
    ~Engine();

    bool Initialize();
    void Run();
    void Shutdown();

private:
    // Core State
    bool isRunning;
    SDL_Window* window;
    SDL_GPUDevice* gpuDevice;

    // Graphics Pipeline & Memory
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* unitQuad;

    SDL_GPUTexture* waterTex;
    SDL_GPUTexture* sandTex;
    SDL_GPUTexture* grassTex;

    SDL_GPUTexture* playerTex;
    SDL_GPUSampler* playerSampler;
    SDL_GPUTexture* crosshairTex;
    SDL_GPUTexture* armTex;
    SDL_GPUTexture* clock17Tex;

    //Audio Logic
    MIX_Mixer* mainMixer = nullptr;
    MIX_Track* footstepTracks[2] = {nullptr, nullptr};

    MIX_Audio* waterSteps[2];
    MIX_Audio* sandSteps[2];
    MIX_Audio* grassSteps[2];

    // Game Logic State
    float playerX, playerY;
    int playerWidth, playerHeight;
    int* mapGrid;
    int mapWidth, mapHeight;

    // Private Sub-routines
    bool SetupPipeline();
    SDL_GPUShader* LoadShader(const char* filepath, SDL_GPUShaderStage stage) const;
    [[nodiscard]] SDL_GPUBuffer* CreateUnitQuad() const;
    SDL_GPUTexture* LoadTextureToGPU(const char* filepath, int* outWidth, int* outHeight) const;

    // Native SVG Loader
    SDL_GPUTexture* LoadSVGToGPU(const char* filepath, float scale, int* outWidth, int* outHeight) const;

    // GAME LOOP HELPERS
    void ProcessInput(Player& player);
    void Render(const Player& player, float mouseX, float mouseY) const;
    void DrawQuad(SDL_GPUCommandBuffer* cmdBuf, SDL_GPURenderPass* renderPass, SDL_GPUTexture* texture, const PushConstants& pushData) const;
};