// created by fasy on 19/04/2026

#include "../Engine.h"
#include <cmath>
#include <fstream>
#include "Player.h"
#include <random>

#include <lunasvg.h>
#include "../json.hpp"

using json = nlohmann::json;

Engine::Engine() :
    isRunning(false), window(nullptr), gpuDevice(nullptr),pipeline(nullptr), unitQuad(nullptr),
    waterTex(nullptr), sandTex(nullptr), grassTex(nullptr),
    activePaletteTex(nullptr), testPaletteTex(nullptr), paletteSamplerObj(nullptr),
    playerTex(nullptr), playerSampler(nullptr), crosshairTex(nullptr),
    armTex(nullptr), ak74Tex(nullptr),
    mainMixer(nullptr),footstepTracks{nullptr, nullptr},
    waterSteps{nullptr, nullptr},sandSteps{nullptr, nullptr},grassSteps{nullptr, nullptr},
    playerX(0.0f), playerY(0.0f),playerWidth(0), playerHeight(0),
    mapGrid(nullptr),mapWidth(0), mapHeight(0),
    activeTracers(), tracer762Tex(nullptr) {
}

Engine::~Engine() {
    Shutdown();
}

bool Engine::Initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init Failed: %s", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("Survive", 1980, 1080, SDL_WINDOW_RESIZABLE);
    if (!window) return false;

    gpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
    if (!gpuDevice) {
        SDL_Log("Vulkan rejected by OS. Attempting fallback...");
        gpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL, false, nullptr);
    }

    if (!gpuDevice || !SDL_ClaimWindowForGPUDevice(gpuDevice, window)) {
        SDL_Log("CRITICAL: GPU Device or Window Claim failed!");
        return false;
    }

    // ldtk map loading
    std::ifstream file("Assets/Level/Test.ldtk");
    if (!file.is_open()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Error", "Failed to load LDtk map! Check your working directory.", nullptr);
        return false;
    }

    json mapData = json::parse(file);
    file.close();

    auto level = mapData["levels"][0];
    auto layer = level["layerInstances"][0];

    mapWidth = layer["__cWid"];
    mapHeight = layer["__cHei"];
    mapGrid = new int[mapWidth * mapHeight];

    auto gridCsv = layer["intGridCsv"];
    for (size_t i = 0; i < gridCsv.size(); i++) {
        mapGrid[i] = gridCsv[i];
    }
    SDL_Log("LDtk Map Loaded: %d x %d tiles", mapWidth, mapHeight);

    // initial player spawn
    playerX = 1980.0f / 2.0f;
    playerY = 1080.0f / 2.0f;

    // load sounds
    if (!MIX_Init()) {
        SDL_Log("MIX_Init failed: %s", SDL_GetError());
    }

    mainMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mainMixer) {
        SDL_Log("MIX_CreateMixer failed: %s", SDL_GetError());
    } else {
        footstepTracks[0] = MIX_CreateTrack(mainMixer);
        footstepTracks[1] = MIX_CreateTrack(mainMixer);
    }

    // load audio
    waterSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/water0.mp3", true);
    waterSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/water1.mp3", true);

    sandSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/sand0.mp3", true);
    sandSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/sand1.mp3", true);

    grassSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/grass0.mp3", true);
    grassSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/grass1.mp3", true);

    // load textures
    activePaletteTex = LoadSVGToGPU("Assets/Player/Skins/Skin_default.svg", 32.0f, nullptr, nullptr);
    testPaletteTex = LoadSVGToGPU("Assets/Player/Skins/Skin_test1.svg", 32.0f, nullptr, nullptr);

    playerTex = LoadSVGToGPU("Assets/Player/Base/Body_mask.svg", 5.0f, &playerWidth, &playerHeight);
    armTex = LoadSVGToGPU("Assets/Player/Base/Arm_mask.svg", 4.0f, nullptr, nullptr);
    crosshairTex = LoadSVGToGPU("Assets/Player/Crosshair.svg", 1.0f, nullptr, nullptr);

    ak74Tex = LoadSVGToGPU("Assets/Player/Guns/Textures/AK74_mask.svg", 5.0f, nullptr, nullptr);

    tracer762Tex = LoadSVGToGPU("Assets/Player/Guns/Tracer/Tracer_762.svg", 1.0f, nullptr, nullptr);

    waterTex = LoadSVGToGPU("Assets/Environment/Water.svg", 1.0f, nullptr, nullptr);
    sandTex = LoadSVGToGPU("Assets/Environment/Sand.svg", 1.0f, nullptr, nullptr);
    grassTex = LoadSVGToGPU("Assets/Environment/Grass.svg", 1.0f, nullptr, nullptr);

    unitQuad = CreateUnitQuad();

    SDL_GPUSamplerCreateInfo samplerInfo = {};
    samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    playerSampler = SDL_CreateGPUSampler(gpuDevice, &samplerInfo);

    SDL_GPUSamplerCreateInfo paletteSamplerInfo = {};
    paletteSamplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    paletteSamplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    paletteSamplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    paletteSamplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    paletteSamplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    paletteSamplerObj = SDL_CreateGPUSampler(gpuDevice, &paletteSamplerInfo);

    // hide os cursor
    SDL_HideCursor();

    if (!SetupPipeline()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Error", "Failed to compile Shaders! Are the .spv files in the current directory?", nullptr);
        return false;
    }

    isRunning = true;
    return true;
}

void Engine::Run() {
    Player player(1980.0f / 2.0f, 1080.0f / 2.0f);

    Weapon ak74(ak74Tex,
            64.0f, 64.0f,
            15.0f, 0.0f,
            0.0f, 0.0f, 1.0f / 6.0f, 1.0f / 5.0f,
            6, 5, 30, 2.5f, 0.1f, 30);

    //temp
    player.SetInventorySlot(0, &ak74);
    player.EquipSlot(0);

    Uint64 lastTime = SDL_GetTicksNS();

    while (isRunning) {
        Uint64 currentTime = SDL_GetTicksNS();
        float deltaTime = static_cast<float>(currentTime - lastTime) / 1e9f;
        lastTime = currentTime;
        ProcessInput(player);

        float mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        float cameraX = player.x - (1980.0f / 2.0f);
        float cameraY = player.y - (1080.0f / 2.0f);

        float worldMouseX = mouseX + cameraX;
        float worldMouseY = mouseY + cameraY;

        player.Update(deltaTime, SDL_GetKeyboardState(nullptr), worldMouseX, worldMouseY, mapGrid, mapWidth, mapHeight);
        Render(player, mouseX, mouseY);

        for (auto it = activeTracers.begin(); it != activeTracers.end(); ) {
            it->life -= deltaTime;
            it->currentX += std::cos(it->angle) * it->speed * deltaTime;
            it->currentY += std::sin(it->angle) * it->speed * deltaTime;

            if (it->life <= 0.0f) {
                it = activeTracers.erase(it);
            } else {
                ++it;
            }
        }

        if (player.justStepped) {
            MIX_Audio* soundToPlay = nullptr;

            if (player.currentTile == 1) soundToPlay = waterSteps[player.stepToggle];
            else if (player.currentTile == 2) soundToPlay = sandSteps[player.stepToggle];
            else if (player.currentTile == 3 || player.currentTile == 0) soundToPlay = grassSteps[player.stepToggle];

            if (soundToPlay != nullptr) {
                MIX_Track* activeTrack = footstepTracks[player.stepToggle];

                if (activeTrack != nullptr) {
                    MIX_SetTrackGain(activeTrack, 1.0f);
                    MIX_SetTrackAudio(activeTrack, soundToPlay);
                    MIX_PlayTrack(activeTrack, false);
                }
            }
        }
    }
}
void Engine::ProcessInput(Player& player) {
    SDL_Event event;
    const bool* keys = SDL_GetKeyboardState(nullptr);

    if (keys[SDL_SCANCODE_1]) player.EquipSlot(0);
    if (keys[SDL_SCANCODE_2]) player.EquipSlot(1);
    if (keys[SDL_SCANCODE_3]) player.EquipSlot(2);

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) isRunning = false;

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                player.Attack();
            }
        }

        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {

            if (keys[SDL_SCANCODE_R]) {
                player.TriggerReload();
            }
            if (event.key.scancode == SDL_SCANCODE_KP_7) {
                player.activeGearLayout = (player.activeGearLayout + 1) % 8;
                SDL_Log("Switched to Gear Layout: %d", player.activeGearLayout);
            }
            if (event.key.scancode == SDL_SCANCODE_KP_8) {
                player.activeSkin = (player.activeSkin + 1) % 2;
                SDL_Log("Switched to Skin: %d", player.activeSkin);
            }
            if (event.key.scancode == SDL_SCANCODE_KP_9) {
                player.armorLevel = (player.armorLevel + 1) % 4;
                SDL_Log("Switched to Armor Level: %d", player.armorLevel);
            }
        }
    }
    Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
    if (mouseState & SDL_BUTTON_LMASK) {
        float gunTipX, gunTipY;
        if (player.AttemptFire(gunTipX, gunTipY)) {
            Tracer newBullet{};
            newBullet.startX = gunTipX;
            newBullet.startY = gunTipY;
            newBullet.currentX = gunTipX;
            newBullet.currentY = gunTipY;

            // FIX: Use modern C++11 random generation
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> spreadDist(-0.025f, 0.025f);
            float spread = spreadDist(rng);

            newBullet.angle = player.armAngle + spread;
            newBullet.speed = 2500.0f;
            newBullet.length = 80.0f;
            newBullet.life = 1.0f;

            activeTracers.push_back(newBullet);
        }
    }
}

void Engine::DrawQuad(SDL_GPUCommandBuffer* cmdBuf, SDL_GPURenderPass* renderPass,
                      SDL_GPUTexture* spriteTex, SDL_GPUTexture* paletteTex,
                      const PushConstants& pushData) const {

    if (spriteTex == nullptr || paletteTex == nullptr) return;

    SDL_GPUTextureSamplerBinding bindings[2] = {
        { spriteTex, playerSampler },
        { paletteTex, paletteSamplerObj }
    };
    SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 2);

    SDL_PushGPUVertexUniformData(cmdBuf, 0, &pushData, sizeof(PushConstants));
    SDL_PushGPUFragmentUniformData(cmdBuf, 0, &pushData, sizeof(PushConstants));

    SDL_DrawGPUPrimitives(renderPass, 6, 1, 0, 0);
}

void Engine::Render(const Player& player, float mouseX, float mouseY) const {
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    if (!cmdBuf) return;

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_AcquireGPUSwapchainTexture(cmdBuf, window, &swapchainTexture, nullptr, nullptr) || !swapchainTexture) {
        SDL_SubmitGPUCommandBuffer(cmdBuf);
        return;
    }

    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = swapchainTexture;
    colorTarget.clear_color = {0.1f, 0.15f, 0.1f, 1.0f};
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    SDL_GPUViewport viewport = { 0.0f, 0.0f, 1980.0f, 1080.0f, 0.0f, 1.0f };
    SDL_SetGPUViewport(renderPass, &viewport);
    SDL_Rect scissor = { 0, 0, 1980, 1080 };
    SDL_SetGPUScissor(renderPass, &scissor);

    SDL_GPUBufferBinding vertexBinding = { unitQuad, 0 };
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

    float cameraX = player.x - (1980.0f / 2.0f);
    float cameraY = player.y - (1080.0f / 2.0f);

    // draw call 0: multi-layer marching squares
    auto drawTerrainLayer = [&](int thresholdValue, SDL_GPUTexture* layerTexture) {

        auto getTile = [&](int gridX, int gridY) -> int {
            if (gridX < 0 || gridX >= mapWidth || gridY < 0 || gridY >= mapHeight) return 0;
            return (mapGrid[gridY * mapWidth + gridX] >= thresholdValue) ? 1 : 0;
        };

        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {

                int topLeft     = getTile(x, y);
                int topRight    = getTile(x + 1, y);
                int bottomLeft  = getTile(x, y + 1);
                int bottomRight = getTile(x + 1, y + 1);

                int tileIndex = 0;
                if (topLeft == 1)     tileIndex |= 8;
                if (topRight == 1)    tileIndex |= 4;
                if (bottomRight == 1) tileIndex |= 2;
                if (bottomLeft == 1)  tileIndex |= 1;

                if (tileIndex != 0) {
                    constexpr float tileSize = 32.0f;
                    float worldX = static_cast<float>(x) * tileSize;
                    float worldY = static_cast<float>(y) * tileSize;

                    float screenX = worldX - cameraX;
                    float screenY = worldY - cameraY;

                    if (screenX < -tileSize || screenX > 1980.0f ||
                        screenY < -tileSize || screenY > 1080.0f) {
                        continue;
                    }

                    int col = tileIndex % 4;
                    int row = tileIndex / 4;

                    float uvWidth = 0.25f;
                    float uvHeight = 0.25f;

                    float calculatedUvX = static_cast<float>(col) * uvWidth;
                    float calculatedUvY = static_cast<float>(row) * uvHeight;

                    PushConstants tileData = {
                        1980.0f, 1080.0f,
                        screenX + (tileSize / 2.0f), screenY + (tileSize / 2.0f),
                        tileSize, tileSize,
                        1.0f, 0.0f,
                        calculatedUvX, calculatedUvY, uvWidth, uvHeight,
            {1.0f, 1.0f, 1.0f, -1.0f}};
                    DrawQuad(cmdBuf, renderPass, layerTexture, layerTexture, tileData);
                }
            }
        }
    };

    // draw terrain
    drawTerrainLayer(1, waterTex);
    drawTerrainLayer(2, sandTex);
    drawTerrainLayer(3, grassTex);

    // draw call 0.5: chunk debug grid
    constexpr float chunkSize = 24.0f * 24.0f;
    constexpr float lineThickness = 2.0f;

    // calculate first visible chunk line
    float startX = std::floor(cameraX / chunkSize) * chunkSize;
    float startY = std::floor(cameraY / chunkSize) * chunkSize;

    float endX = cameraX + 1980.0f;
    float endY = cameraY + 1080.0f;

    // draw vertical chunk lines
    int startCol = static_cast<int>(std::floor(startX / chunkSize));
    int endCol = static_cast<int>(std::ceil(endX / chunkSize));


    for (int i = startCol; i <= endCol; ++i) {
        float x = static_cast<float>(i) * chunkSize;
        float screenX = x - cameraX;

        PushConstants verticalLine = {
            1980.0f, 1080.0f,
            screenX, 1080.0f / 2.0f,
            lineThickness, 1080.0f,
            1.0f, 0.0f,

            0.5f, 0.5f, 0.0f, 0.0f,

            {0.5f, 0.5f, 0.5f, -1.0f}
        };
        DrawQuad(cmdBuf, renderPass, crosshairTex, crosshairTex, verticalLine);
    }

    // draw horizontal chunk lines
    int startRow = static_cast<int>(std::floor(startY / chunkSize));
    int endRow = static_cast<int>(std::ceil(endY / chunkSize));

    for (int i = startRow; i <= endRow; ++i) {
        float y = static_cast<float>(i) * chunkSize;
        float screenY = y - cameraY;

        PushConstants horizontalLine = {
            1980.0f, 1080.0f,
            1980.0f / 2.0f, screenY,
            1980.0f, lineThickness,
            1.0f, 0.0f,

            0.5f, 0.5f, 0.0f, 0.0f,

            {0.5f, 0.5f, 0.5f, -1.0f}
        };
        DrawQuad(cmdBuf, renderPass, crosshairTex, crosshairTex, horizontalLine);
    }

    float drawAngle = player.armAngle + 1.570796f;
    // DRAW CALL 1: BODY
    float bodyUvW = 0.25f;
    float bodyUvH = 0.5f;

    constexpr float ARMOR_PALETTE[4][4] = {
        { 0.0f, 0.0f, 0.0f, 0.0f },   // Lvl 0
        { 0.33f, 0.33f, 0.33f, 1.0f },// Lvl 1
        { 0.66f, 0.66f, 0.66f, 1.0f },// Lvl 2
        { 0.1f, 0.1f, 0.1f, 1.0f }    // Lvl 3
    };

    float currentArmorColor[4] = {
        ARMOR_PALETTE[player.armorLevel][0],
        ARMOR_PALETTE[player.armorLevel][1],
        ARMOR_PALETTE[player.armorLevel][2],
        ARMOR_PALETTE[player.armorLevel][3]
    };

    int gearRow = player.activeGearLayout / 4;
    int gearCol = player.activeGearLayout % 4;

    float bodyUvX = static_cast<float>(gearCol) * bodyUvW;
    float bodyUvY = static_cast<float>(gearRow) * bodyUvH;

    SDL_GPUTexture* currentSkinTex = (player.activeSkin == 0) ? activePaletteTex : testPaletteTex;

    PushConstants bodyData = {
        1980.0f, 1080.0f,
        player.x - cameraX, player.y - cameraY,
        64.0f, 64.0f,
        std::cos(drawAngle), std::sin(drawAngle),
        bodyUvX, bodyUvY, bodyUvW, bodyUvH,
        { currentArmorColor[0], currentArmorColor[1], currentArmorColor[2], currentArmorColor[3] }
    };

    DrawQuad(cmdBuf, renderPass, playerTex, currentSkinTex, bodyData);

    // ---------------------------------------------------------
    // DRAW CALL 2: WEAPON OR UNARMED (SLOT 3)
    // ---------------------------------------------------------
    if (player.equippedWeapon != nullptr) {
        // --- EQUIPPED WEAPON LOGIC ---
        float recoil = 0.0f;
        int animFrame = 0;

        int cols = player.equippedWeapon->animCols;
        int rows = player.equippedWeapon->animRows;
        int totalFrames = player.equippedWeapon->totalAnimFrames;

        if (player.isAttacking) {
            animFrame = player.currentFrame % totalFrames;
            if (animFrame == 1) recoil = -10.0f;
            if (animFrame == 2) recoil = -5.0f;
        }

        float dist = 35.0f + recoil;
        float gunX = player.x + (std::cos(player.armAngle) * (dist + player.equippedWeapon->gripOffsetX));
        float gunY = player.y + (std::sin(player.armAngle) * (dist + player.equippedWeapon->gripOffsetX));

        float frameWidth = 1.0f / static_cast<float>(cols);
        float frameHeight = 1.0f / static_cast<float>(rows);

        int animRow = animFrame / cols;
        int animCol = animFrame % cols;

        float currentUvX = static_cast<float>(animCol) * frameWidth;
        float currentUvY = static_cast<float>(animRow) * frameHeight;

        PushConstants weaponData = {
            1980.0f, 1080.0f,
            gunX - cameraX, gunY - cameraY,
            player.equippedWeapon->width, player.equippedWeapon->height,
            std::cos(drawAngle), std::sin(drawAngle),
            currentUvX, currentUvY, frameWidth, frameHeight,
            { currentArmorColor[0], currentArmorColor[1], currentArmorColor[2], 1.0f }
        };

        DrawQuad(cmdBuf, renderPass, player.equippedWeapon->texture, currentSkinTex, weaponData);
    } else {
        // --- SLOT 3 (ALTERNATING VIA currentCombo) ---
        int armFrame = 0;

        if (player.isAttacking) {
            if (player.currentCombo == 1) {
                // Right Arm: Uses SVG frames 0, 1, 2, 3
                armFrame = player.currentFrame;
            } else {
                // Left Arm: Uses SVG frames 0, 4, 5, 6
                // Frame 0 is shared Idle, then skip to the Left Hook frames
                armFrame = (player.currentFrame == 0) ? 0 : player.currentFrame + 3;
            }
        }

        float totalArmFrames = 7.0f; //
        float handDist = 25.0f;

        float handX = player.x + (std::cos(player.armAngle) * handDist);
        float handY = player.y + (std::sin(player.armAngle) * handDist);

        float armUvW = 1.0f / totalArmFrames;
        float armUvH = 1.0f;
        float currentArmUvX = (static_cast<float>(armFrame) * armUvW) + 0.0001f;

        PushConstants armData = {
            1980.0f, 1080.0f,
            handX - cameraX, handY - cameraY,
            64.0f, 64.0f,
            std::cos(drawAngle), std::sin(drawAngle),
            currentArmUvX, 0.0f, armUvW, armUvH,
            { currentArmorColor[0], currentArmorColor[1], currentArmorColor[2], 1.0f }
        };

        DrawQuad(cmdBuf, renderPass, armTex, currentSkinTex, armData);
    }

    // ---------------------------------------------------------
    // DRAW CALL 2.5: TRACERS (Restored!)
    // ---------------------------------------------------------
    for (const auto& tracer : activeTracers) {
        PushConstants tracerData = {
            1980.0f, 1080.0f,
            tracer.currentX - cameraX, tracer.currentY - cameraY,
            tracer.length, 8.0f,
            std::cos(tracer.angle), std::sin(tracer.angle),
            0.0f, 0.0f, 1.0f, 1.0f,
            {1.0f, 1.0f, 1.0f, -1.0f} // -1.0f bypasses the dye shader
        };
        DrawQuad(cmdBuf, renderPass, tracer762Tex, tracer762Tex, tracerData);
    }

    // draw call 3: crosshair
    PushConstants crosshairData = {
        1980.0f, 1080.0f,
        mouseX, mouseY,
        32.0f, 32.0f,
        1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        {1.0f, 1.0f, 1.0f, -1.0f}
    };
    DrawQuad(cmdBuf, renderPass, crosshairTex, crosshairTex, crosshairData);

    SDL_EndGPURenderPass(renderPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
}

void Engine::Shutdown() {
    if (mapGrid) {
        delete[] mapGrid;
        mapGrid = nullptr;
    }

    if (gpuDevice) {
        SDL_WaitForGPUIdle(gpuDevice);

        if (pipeline) SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
        if (playerSampler) SDL_ReleaseGPUSampler(gpuDevice, playerSampler);
        if (unitQuad) SDL_ReleaseGPUBuffer(gpuDevice, unitQuad);

        if (playerTex) SDL_ReleaseGPUTexture(gpuDevice, playerTex);
        if (crosshairTex) SDL_ReleaseGPUTexture(gpuDevice, crosshairTex);
        if (armTex) SDL_ReleaseGPUTexture(gpuDevice, armTex);

        if (ak74Tex) SDL_ReleaseGPUTexture(gpuDevice, ak74Tex);

        if (tracer762Tex) SDL_ReleaseGPUTexture(gpuDevice, tracer762Tex);

        if (waterTex) SDL_ReleaseGPUTexture(gpuDevice, waterTex);
        if (grassTex) SDL_ReleaseGPUTexture(gpuDevice, grassTex);
        if (sandTex) SDL_ReleaseGPUTexture(gpuDevice, sandTex);

        for (int i=0; i<2; i++) {
            if (waterSteps[i]) MIX_DestroyAudio(waterSteps[i]);
            if (sandSteps[i]) MIX_DestroyAudio(sandSteps[i]);
            if (grassSteps[i]) MIX_DestroyAudio(grassSteps[i]);
        }
        if (mainMixer) MIX_DestroyMixer(mainMixer);
        MIX_Quit();

        if (window) SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
        SDL_DestroyGPUDevice(gpuDevice);
    }

    if (window) SDL_DestroyWindow(window);
    SDL_ShowCursor();
    SDL_Quit();
}

bool Engine::SetupPipeline() {
    SDL_GPUShader* vertShader = LoadShader("sprite.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader* fragShader = LoadShader("sprite.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT);

    if (!vertShader || !fragShader) return false;

    SDL_GPUColorTargetDescription colorTargetDesc = {};
    colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window);
    colorTargetDesc.blend_state.enable_blend = true;
    colorTargetDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTargetDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    colorTargetDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorTargetDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorTargetDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTargetDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.vertex_shader = vertShader;
    pipelineInfo.fragment_shader = fragShader;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;

    SDL_GPUVertexBufferDescription vertexBufferDesc = {};
    vertexBufferDesc.slot = 0;
    vertexBufferDesc.pitch = sizeof(Vertex);
    vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute vertexAttributes[2] = {};
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[0].offset = offsetof(Vertex, x);
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertexAttributes[1].offset = offsetof(Vertex, u);

    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDesc;
    pipelineInfo.vertex_input_state.num_vertex_attributes = 2;
    pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

    pipeline = SDL_CreateGPUGraphicsPipeline(gpuDevice, &pipelineInfo);

    SDL_ReleaseGPUShader(gpuDevice, vertShader);
    SDL_ReleaseGPUShader(gpuDevice, fragShader);

    return pipeline != nullptr;
}

SDL_GPUShader* Engine::LoadShader(const char* filepath, SDL_GPUShaderStage stage) const {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return nullptr;

    size_t fileSize = (size_t)file.tellg();
    char* buffer = new char[fileSize];
    file.seekg(0);
    file.read(buffer, static_cast<std::streamsize>(fileSize));
    file.close();

    SDL_GPUShaderCreateInfo shaderInfo = {};
    shaderInfo.stage = stage;
    shaderInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    shaderInfo.code = reinterpret_cast<const Uint8 *>(buffer);
    shaderInfo.code_size = fileSize;
    shaderInfo.entrypoint = "main";

    if (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        shaderInfo.num_samplers = 2;
        shaderInfo.num_uniform_buffers = 1;
    }
    if (stage == SDL_GPU_SHADERSTAGE_VERTEX) {
        shaderInfo.num_uniform_buffers = 1;
    }

    SDL_GPUShader* finalShader = SDL_CreateGPUShader(gpuDevice, &shaderInfo);

    delete[] buffer;
    return finalShader;
}

SDL_GPUBuffer* Engine::CreateUnitQuad() const {
    Vertex quad[6] = {
        {0.0f, 0.0f,  0.0f, 0.0f}, {0.0f, 1.0f,  0.0f, 1.0f}, {1.0f, 0.0f,  1.0f, 0.0f},
        {1.0f, 0.0f,  1.0f, 0.0f}, {0.0f, 1.0f,  0.0f, 1.0f}, {1.0f, 1.0f,  1.0f, 1.0f}
    };
    Uint32 bufferSize = sizeof(quad);

    SDL_GPUBufferCreateInfo bufferInfo = {};
    bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bufferInfo.size = bufferSize;
    SDL_GPUBuffer* gpuBuffer = SDL_CreateGPUBuffer(gpuDevice, &bufferInfo);

    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = bufferSize;
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);

    void* map = SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false);
    memcpy(map, quad, bufferSize);
    SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTransferBufferLocation source = { transferBuffer, 0 };
    SDL_GPUBufferRegion dest = { gpuBuffer, 0, bufferSize };

    SDL_UploadToGPUBuffer(copyPass, &source, &dest, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_WaitForGPUIdle(gpuDevice);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);

    return gpuBuffer;
}

SDL_GPUTexture* Engine::LoadSVGToGPU(const char* filepath, float scale, int* outWidth, int* outHeight) const {
    auto document = lunasvg::Document::loadFromFile(filepath);
    if (!document) {
        SDL_Log("CRITICAL: Failed to load SVG file: %s", filepath);
        return nullptr;
    }

    double docW = document->width();
    double docH = document->height();

    // 1. Handle missing/explicit dimensions
    if (docW <= 0.0 || docH <= 0.0) {
        lunasvg::Bitmap dummy = document->renderToBitmap(100, 100);
        docW = dummy.width();
        docH = dummy.height();
    }

    // 2. Clamp scale to prevent negative/zero/massive dimensions
    if (scale <= 0.0f) scale = 1.0f;

    // 3. Calculate target dimensions safely (Clang-Tidy: Use auto with cast)
    auto finalWidth  = static_cast<uint32_t>(docW * scale);
    auto finalHeight = static_cast<uint32_t>(docH * scale);

    if (finalWidth < 1) finalWidth = 1;
    if (finalHeight < 1) finalHeight = 1;

    // 4. Apply safety cap (Clang-Tidy: constexpr)
    constexpr uint32_t MAX_TEX_SIZE = 4096;
    if (finalWidth > MAX_TEX_SIZE || finalHeight > MAX_TEX_SIZE) {
        // Clang-Tidy: Use double to prevent precision loss (narrowing) warning
        double maxDim = std::max(static_cast<double>(finalWidth), static_cast<double>(finalHeight));
        double limitScale = static_cast<double>(MAX_TEX_SIZE) / maxDim;

        finalWidth  = static_cast<uint32_t>(static_cast<double>(finalWidth) * limitScale);
        finalHeight = static_cast<uint32_t>(static_cast<double>(finalHeight) * limitScale);

        if (finalWidth < 1) finalWidth = 1;
        if (finalHeight < 1) finalHeight = 1;
    }

    // 5. Render directly at the safe target size
    lunasvg::Bitmap bitmap = document->renderToBitmap(finalWidth, finalHeight);
    if (bitmap.isNull() || !bitmap.data()) {
        SDL_Log("WARNING: Failed to render SVG to bitmap for %s", filepath);
        return nullptr;
    }

    finalWidth = bitmap.width();
    finalHeight = bitmap.height();

    // Bitwise AND 0x7FFFFFFF guarantees it fits in a signed int, silencing the narrowing warning
    if (outWidth) *outWidth = static_cast<int>(finalWidth & 0x7FFFFFFF);
    if (outHeight) *outHeight = static_cast<int>(finalHeight & 0x7FFFFFFF);

    // 6. Create Texture
    SDL_GPUTextureCreateInfo textureInfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = finalWidth,
        .height = finalHeight,
        .layer_count_or_depth = 1,
        .num_levels = 1
    };
    SDL_GPUTexture* gpuTexture = SDL_CreateGPUTexture(gpuDevice, &textureInfo);
    if (!gpuTexture) {
        SDL_Log("CRITICAL: Failed to create GPU texture for %s", filepath);
        return nullptr;
    }

    // 7. Create Transfer Buffer
    Uint32 imageSize = finalWidth * finalHeight * 4;
    SDL_GPUTransferBufferCreateInfo transferInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = imageSize
    };
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);
    if (!transferBuffer) {
        SDL_Log("CRITICAL: Failed to create transfer buffer for %s", filepath);
        SDL_ReleaseGPUTexture(gpuDevice, gpuTexture); // FIX: Changed from Destroy to Release
        return nullptr;
    }

    // 8. Map and swizzle from LunaSVG (BGRA) to GPU (RGBA)
    // Clang-Tidy: Moved 'map' declaration inside the if-statement (C++17)
    if (auto* map = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false))) {
        for (uint32_t y = 0; y < finalHeight; ++y) {
            const uint8_t* srcRow = bitmap.data() + (y * bitmap.stride());
            uint8_t* destRow = map + (y * finalWidth * 4);

            for (uint32_t x = 0; x < finalWidth; ++x) {
                destRow[x * 4 + 0] = srcRow[x * 4 + 2]; // R
                destRow[x * 4 + 1] = srcRow[x * 4 + 1]; // G
                destRow[x * 4 + 2] = srcRow[x * 4 + 0]; // B
                destRow[x * 4 + 3] = srcRow[x * 4 + 3]; // A
            }
        }
        SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);
    }

    // 9. Submit to GPU
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    if (!cmdBuf) {
        SDL_ReleaseGPUTexture(gpuDevice, gpuTexture); // FIX: Changed from Destroy to Release
        SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
        return nullptr;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTextureTransferInfo source = {
        .transfer_buffer = transferBuffer,
        .offset = 0,
        .pixels_per_row = finalWidth,
        .rows_per_layer = finalHeight
    };

    SDL_GPUTextureRegion dest = {
        .texture = gpuTexture,
        .w = finalWidth,
        .h = finalHeight,
        .d = 1
    };

    SDL_UploadToGPUTexture(copyPass, &source, &dest, false);
    SDL_EndGPUCopyPass(copyPass);

    // FIX: Restored the missing submission and return statement
    SDL_SubmitGPUCommandBuffer(cmdBuf);

    SDL_WaitForGPUIdle(gpuDevice);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);

    return gpuTexture;
}