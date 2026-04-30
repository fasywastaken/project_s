//
// Created by Fasy on 19/04/2026.
//

#include "Engine.h"
#include <cmath>
#include <fstream>
#include "Player.h"

// Image Loading Implementations
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#include "json.hpp"
using json = nlohmann::json;

Engine::Engine() : isRunning(false), window(nullptr), gpuDevice(nullptr),
                   pipeline(nullptr), unitQuad(nullptr), waterTex(nullptr), sandTex(nullptr), grassTex(nullptr),
                   playerTex(nullptr),
                   playerSampler(nullptr), crosshairTex(nullptr), armTex(nullptr), clock17Tex(nullptr), waterSteps{},
                   sandSteps{},
                   grassSteps{},
                   playerX(0.0f),
                   playerY(0.0f),
                   playerWidth(0), playerHeight(0), mapGrid(nullptr),
                   mapWidth(0), mapHeight(0) {
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

    // --- LDTK MAP LOADING ---
    std::ifstream file("Assets/Level/Test.ldtk");
    if (!file.is_open()) {
        SDL_Log("CRITICAL: Failed to load LDtk map! Check your file path.");
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

    // Initial Player Spawn
    playerX = 1980.0f / 2.0f;
    playerY = 1080.0f / 2.0f;



    // Load Sounds
    if (MIX_Init()) {
        SDL_Log("MIX_Init failed: %s", SDL_GetError());
    }

    mainMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mainMixer) {
        SDL_Log("MIX_CreateMixer failed: %s", SDL_GetError());
    } else {
        footstepTracks[0] = MIX_CreateTrack(mainMixer); // Left Foot
        footstepTracks[1] = MIX_CreateTrack(mainMixer); // Right Foot
    }

    // 4. Load Audio using the new MIX_LoadAudio function
    waterSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/water0.mp3", true);
    waterSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/water1.mp3", true);

    sandSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/sand0.mp3", true);
    sandSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/sand1.mp3", true);

    grassSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/grass0.mp3", true);
    grassSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/grass1.mp3", true);

    // Load Textures

    playerTex = LoadSVGToGPU("Assets/Player/Body.svg", 1.0f, &playerWidth, &playerHeight);
    armTex = LoadTextureToGPU("Assets/Player/Arm.png", nullptr, nullptr);  //CHANGE TO SVG
    clock17Tex = LoadTextureToGPU("Assets/Player/Guns/Textures/Clock17-Sprites.png", nullptr, nullptr);
    crosshairTex = LoadSVGToGPU("Assets/Player/Crosshair.svg", 1.0f, nullptr, nullptr); // CHANGE TO SVG

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
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    playerSampler = SDL_CreateGPUSampler(gpuDevice, &samplerInfo);

    SDL_HideCursor(); // Hide OS Cursor

    if (!SetupPipeline()) {
        return false;
    }

    isRunning = true;
    return true;
}

void Engine::Run() {
    Player player(1980.0f / 2.0f, 1080.0f / 2.0f);
    Weapon clock17(clock17Tex, 96.0f, 96.0f, 2.5f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f);

    player.SetInventorySlot(0, &clock17);
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
    }
}

void Engine::DrawQuad(SDL_GPUCommandBuffer* cmdBuf, SDL_GPURenderPass* renderPass, SDL_GPUTexture* texture, const PushConstants& pushData) const {
    if (texture == nullptr) {
        return;
    }

    SDL_GPUTextureSamplerBinding binding = { texture, playerSampler };
    SDL_BindGPUFragmentSamplers(renderPass, 0, &binding, 1);

    SDL_PushGPUVertexUniformData(cmdBuf, 0, &pushData, sizeof(PushConstants));
    SDL_PushGPUFragmentUniformData(cmdBuf, 0, &pushData, sizeof(PushConstants)); // <-- NEW!

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

// ==========================================
    // DRAW CALL 0: MULTI-LAYER MARCHING SQUARES
    // ==========================================

    // We wrap the marching squares logic in a lambda so we can call it for each layer!
    auto drawTerrainLayer = [&](int thresholdValue, SDL_GPUTexture* layerTexture) {

        // THE SECRET TRICK: If a tile is EQUAL TO or HIGHER than our current layer,
        // we treat it as "Solid" (1). This makes Sand run underneath Grass seamlessly!
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
                        {1.0f, 1.0f, 1.0f, 1.0f}
                    };

                    DrawQuad(cmdBuf, renderPass, layerTexture, tileData);
                }
            }
        }
    };
    //Draw Terrain
    drawTerrainLayer(1, waterTex);
    drawTerrainLayer(2, sandTex);
    drawTerrainLayer(3, grassTex);

    // ==========================================
    // DRAW CALL 0.5: CHUNK DEBUG GRID
    // ==========================================
    
    constexpr float chunkSize = 24.0f * 24.0f;
    constexpr float lineThickness = 2.0f;

    // Calculate the first visible chunk line on the screen
    float startX = std::floor(cameraX / chunkSize) * chunkSize;
    float startY = std::floor(cameraY / chunkSize) * chunkSize;

    float endX = cameraX + 1980.0f;
    float endY = cameraY + 1080.0f;

    // --- DRAW VERTICAL CHUNK LINES ---
    for (float x = startX; x <= endX; x += chunkSize) {
        float screenX = x - cameraX;

        PushConstants verticalLine = {
            1980.0f, 1080.0f,
            screenX, 1080.0f / 2.0f,
            lineThickness, 1080.0f,
            1.0f, 0.0f,

            // Sample the dead-center of the crosshair (solid white pixel)
            0.5f, 0.5f, 0.0f, 0.0f,

            // THE COLOR TINT: {R, G, B, Alpha} -> Semi-transparent Gray
            {0.5f, 0.5f, 0.5f, 0.5f}
        };
        DrawQuad(cmdBuf, renderPass, crosshairTex, verticalLine);
    }

    // --- DRAW HORIZONTAL CHUNK LINES ---
    for (float y = startY; y <= endY; y += chunkSize) {
        float screenY = y - cameraY;

        PushConstants horizontalLine = {
            1980.0f, 1080.0f,
            1980.0f / 2.0f, screenY,
            1980.0f, lineThickness,
            1.0f, 0.0f,

            0.5f, 0.5f, 0.0f, 0.0f,

            // THE COLOR TINT: {R, G, B, Alpha} -> Semi-transparent Gray
            {0.0f, 0.0f, 0.0f, 0.15f}
        };
        DrawQuad(cmdBuf, renderPass, crosshairTex, horizontalLine);
    }

    // --- DRAW CALL 1: BODY ---
    PushConstants bodyData = {
        1980.0f, 1080.0f,
        player.x - cameraX, player.y - cameraY,
        64.0f, 64.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        {1.0f, 1.0f, 1.0f, 1.0f}
    };
    DrawQuad(cmdBuf, renderPass, playerTex, bodyData);

    // --- DRAW CALL 2: ARMS/WEAPON ---
    float drawAngle = player.armAngle + 1.570796f;
    bool aimingLeft = std::cos(player.armAngle) < 0.0f;
    float flipX = aimingLeft ? -1.0f : 1.0f;

    if (player.equippedWeapon != nullptr) {
        float recoil = 0.0f;
        if (player.isAttacking) {
            if (player.currentFrame == 1) recoil = -10.0f;
            if (player.currentFrame == 2) recoil = -5.0f;
        }

        float dist = 35.0f + recoil;
        float gunX = player.x + (std::cos(player.armAngle) * (dist + player.equippedWeapon->gripOffsetX));
        float gunY = player.y + (std::sin(player.armAngle) * (dist + player.equippedWeapon->gripOffsetX));
        float currentUvX = player.equippedWeapon->uvX + (player.equippedWeapon->isAkimbo ? player.equippedWeapon->uvW : 0.0f);

        PushConstants weaponData = {
            1980.0f, 1080.0f,
            gunX - cameraX, gunY - cameraY,
            player.equippedWeapon->width * flipX, player.equippedWeapon->height,
            std::cos(drawAngle), std::sin(drawAngle),
            currentUvX, player.equippedWeapon->uvY, player.equippedWeapon->uvW, player.equippedWeapon->uvH,
            {1.0f, 1.0f, 1.0f, 1.0f}
        };
        DrawQuad(cmdBuf, renderPass, player.equippedWeapon->texture, weaponData);

    } else {
        float dist = 25.0f;
        float armX = player.x + (std::cos(player.armAngle) * dist);
        float armY = player.y + (std::sin(player.armAngle) * dist);

        auto mappedFrame = static_cast<float>(player.currentFrame);
        if (player.currentCombo == 2 && player.isAttacking) {
            mappedFrame += 3.0f;
        }

        float frameWidth = 1.0f / 7.0f;
        float currentUvX = mappedFrame * frameWidth;

        PushConstants armData = {
            1980.0f, 1080.0f,
            armX - cameraX, armY - cameraY,
            96.0f * flipX, 96.0f,
            std::cos(drawAngle), std::sin(drawAngle),
            currentUvX, 0.0f, frameWidth, 1.0f,
            {1.0f, 1.0f, 1.0f, 1.0f}
        };
        DrawQuad(cmdBuf, renderPass, armTex, armData);
    }

    // --- DRAW CALL 3: CROSSHAIR ---
    PushConstants crosshairData = {
        1980.0f, 1080.0f,
        mouseX, mouseY, // Raw Screen Coordinates!
        32.0f, 32.0f,
        1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        {1.0f, 1.0f, 1.0f, 1.0f}
    };
    DrawQuad(cmdBuf, renderPass, crosshairTex, crosshairData);

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
        //engine
        if (pipeline) SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
        if (playerSampler) SDL_ReleaseGPUSampler(gpuDevice, playerSampler);
        if (unitQuad) SDL_ReleaseGPUBuffer(gpuDevice, unitQuad);
        //player
        if (playerTex) SDL_ReleaseGPUTexture(gpuDevice, playerTex);
        if (crosshairTex) SDL_ReleaseGPUTexture(gpuDevice, crosshairTex);
        if (armTex) SDL_ReleaseGPUTexture(gpuDevice, armTex);
        //weapons
        if (clock17Tex) SDL_ReleaseGPUTexture(gpuDevice, clock17Tex);

        //textures
        if (waterTex) SDL_ReleaseGPUTexture(gpuDevice, waterTex);
        if (grassTex) SDL_ReleaseGPUTexture(gpuDevice, grassTex);
        if (sandTex) SDL_ReleaseGPUTexture(gpuDevice, sandTex);

        //audio
        for (int i=0; i<2; i++) {
            if (waterSteps[i]) MIX_DestroyAudio(waterSteps[i]);
            if (sandSteps[i]) MIX_DestroyAudio(sandSteps[i]);
            if (grassSteps[i]) MIX_DestroyAudio(grassSteps[i]);
        }
        if (mainMixer) MIX_DestroyMixer(mainMixer);
        MIX_Quit();

        //window
        if (window) SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
        SDL_DestroyGPUDevice(gpuDevice);
    }

    if (window) SDL_DestroyWindow(window);
    SDL_ShowCursor();
    SDL_Quit();
}

// ... [SetupPipeline, LoadShader, and CreateUnitQuad remain exactly the same] ...
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
        shaderInfo.num_samplers = 1;
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

SDL_GPUTexture* Engine::LoadTextureToGPU(const char* filepath, int* outWidth, int* outHeight) const {
    int width, height, channels;
    unsigned char* pixels = stbi_load(filepath, &width, &height, &channels, 4);
    if (!pixels) return nullptr;

    if (outWidth) *outWidth = width;
    if (outHeight) *outHeight = height;

    SDL_GPUTextureCreateInfo textureInfo = {};
    textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureInfo.width = width;
    textureInfo.height = height;
    textureInfo.layer_count_or_depth = 1;
    textureInfo.num_levels = 1;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture* gpuTexture = SDL_CreateGPUTexture(gpuDevice, &textureInfo);
    Uint32 imageSize = width * height * 4;

    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = imageSize;
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);

    void* map = SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false);
    memcpy(map, pixels, imageSize);
    SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);
    stbi_image_free(pixels);

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTextureTransferInfo source = { transferBuffer, 0 };
    SDL_GPUTextureRegion dest = {};
    dest.texture = gpuTexture;
    dest.w = static_cast<Uint32>(width);
    dest.h = static_cast<Uint32>(height);
    dest.d = 1;

    SDL_UploadToGPUTexture(copyPass, &source, &dest, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_WaitForGPUIdle(gpuDevice);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);

    return gpuTexture;
}

SDL_GPUTexture* Engine::LoadSVGToGPU(const char* filepath, float scale, int* outWidth, int* outHeight) const {
    NSVGimage* svgImage = nsvgParseFromFile(filepath, "px", 96.0f);
    if (!svgImage) {
        SDL_Log("Failed to load SVG: %s", filepath);
        return nullptr;
    }

    int width = static_cast<int>(svgImage->width * scale);
    int height = static_cast<int>(svgImage->height * scale);

    if (outWidth) *outWidth = width;
    if (outHeight) *outHeight = height;

    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    auto pixels = new unsigned char[width * height * 4];

    memset(pixels, 0, width * height * 4);

    nsvgRasterize(rasterizer, svgImage, 0, 0, scale, pixels, width, height, width * 4);

    SDL_GPUTextureCreateInfo textureInfo = {};
    textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureInfo.width = width;
    textureInfo.height = height;
    textureInfo.layer_count_or_depth = 1;
    textureInfo.num_levels = 1;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    SDL_GPUTexture* gpuTexture = SDL_CreateGPUTexture(gpuDevice, &textureInfo);
    Uint32 imageSize = width * height * 4;

    SDL_GPUTransferBufferCreateInfo transferInfo = {};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = imageSize;
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);

    void* map = SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false);
    memcpy(map, pixels, imageSize);
    SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);

    delete[] pixels;
    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(svgImage);

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

    SDL_GPUTextureTransferInfo source = { transferBuffer, 0 };
    SDL_GPUTextureRegion dest = {};
    dest.texture = gpuTexture;
    dest.w = static_cast<Uint32>(width);
    dest.h = static_cast<Uint32>(height);
    dest.d = 1;

    SDL_UploadToGPUTexture(copyPass, &source, &dest, false);
    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmdBuf);
    SDL_WaitForGPUIdle(gpuDevice);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);

    return gpuTexture;
}