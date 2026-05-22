// created by fasy on 19/04/2026

#include "Engine.h"
#include <cmath>
#include <fstream>
#include "Player.h"
#include <random>

#include <resvg.h>
#include <cstring>
#include "json.hpp"

#define FNL_IMPL
#include "FastNoiseLite.h"

Engine::Engine() : isRunning(false), window(nullptr), gpuDevice(nullptr), pipeline(nullptr), unitQuad(nullptr),
                   waterTex(nullptr), sandTex(nullptr), grassTex(nullptr), treeTex(nullptr), paletteSampler(nullptr),
                   linearSampler(nullptr),
                   playerTex(nullptr), nearestSampler(nullptr), crosshairTex(nullptr),
                   armTex(nullptr), ak74Tex(nullptr),
                   waterSteps{nullptr, nullptr}, sandSteps{nullptr, nullptr}, grassSteps{nullptr, nullptr},
                   weaponFireTrack(nullptr),
                   weaponMechTrack(nullptr),
                   playerTrack(nullptr),
                   punch_swing(nullptr),
                   ak47_switch(nullptr),
                   ak47_fire(nullptr),
                   ak47_reload(nullptr),
                   playerX(0.0f), playerY(0.0f), playerWidth(0), playerHeight(0),
                   mapGrid(nullptr), mapWidth(0), mapHeight(0), enemyTracks{}, spiderTex(nullptr), spiderSteps{},
                   tracer762Tex(nullptr) {
}

Engine::~Engine() {
    Shutdown();
}

int Engine::GetTileValue(int x, int y, const int* grid, int w, int h) {
    if (x < 0 || x >= w || y < 0 || y >= h) {
        return 3; // treat out-of-bounds as grass for blur purposes
    }
    return grid[y * w + x];
}

void Engine::GenerateMap(int width, int height) {
    mapWidth  = width;
    mapHeight = height;
    delete[] mapGrid;
    mapGrid = new int[static_cast<size_t>(mapWidth) * mapHeight];

    std::random_device rd;
    const int seedBase = static_cast<int>(rd());
    std::mt19937 gen(static_cast<unsigned>(seedBase));

    auto warp = fnlCreateState();
    warp.seed               = seedBase + 42;
    warp.domain_warp_type   = FNL_DOMAIN_WARP_OPENSIMPLEX2;
    warp.domain_warp_amp    = 120.0f;
    warp.frequency          = 0.003f;

    auto continental = fnlCreateState();
    continental.seed         = seedBase;
    continental.noise_type   = FNL_NOISE_OPENSIMPLEX2;
    continental.frequency    = 0.0025f;
    continental.fractal_type = FNL_FRACTAL_FBM;
    continental.octaves      = 5;

    auto detail = fnlCreateState();
    detail.seed       = seedBase + 1337;
    detail.noise_type = FNL_NOISE_OPENSIMPLEX2;
    detail.frequency  = 0.02f;

    auto densityNoise = fnlCreateState();
    densityNoise.seed       = seedBase + 999;
    densityNoise.noise_type = FNL_NOISE_OPENSIMPLEX2;
    densityNoise.frequency  = 0.005f;

    const size_t totalCells = static_cast<size_t>(mapWidth) * mapHeight;
    std::vector<float> heightMap(totalCells);

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            auto nx = static_cast<float>(x - mapWidth  / 2);
            auto ny = static_cast<float>(y - mapHeight / 2);
            fnlDomainWarp2D(&warp, &nx, &ny);

            float continent   = fnlGetNoise2D(&continental, nx, ny);
            float detailNoise = fnlGetNoise2D(&detail, nx, ny) * 0.15f;
            float h = (continent * 0.85f) + detailNoise;
            h = (h + 1.0f) * 0.5f;

            float dx = static_cast<float>(x - mapWidth  / 2) / (mapWidth  / 2.0f);
            float dy = static_cast<float>(y - mapHeight / 2) / (mapHeight / 2.0f);
            float dist = std::clamp(std::sqrt(dx*dx + dy*dy), 0.0f, 1.0f);
            float falloff = 1.0f - (dist * dist * dist);
            h = h * falloff;

            const size_t idx = static_cast<size_t>(y) * mapWidth + x;
            heightMap[idx]   = std::clamp(h, 0.0f, 1.0f);
        }
    }

    // FIX: Expanded radius from -2/2 to -6/6.
    // This creates a 13x13 island buffer that easily survives the upcoming blur passes.
    const int cx = mapWidth  / 2;
    const int cy = mapHeight / 2;
    for (int sy = -6; sy <= 6; ++sy) {
        for (int sx = -6; sx <= 6; ++sx) {
            int targetX = std::clamp(cx + sx, 0, mapWidth - 1);
            int targetY = std::clamp(cy + sy, 0, mapHeight - 1);
            size_t idx = static_cast<size_t>(targetY) * mapWidth + targetX;
            heightMap[idx] = std::max(heightMap[idx], 0.55f);
        }
    }

    for (size_t i = 0; i < totalCells; ++i) {
        float h = heightMap[i];
        if      (h < 0.38f) mapGrid[i] = 1; // water
        else if (h < 0.43f) mapGrid[i] = 2; // sand
        else                mapGrid[i] = 3; // grass
    }

    std::vector<int> blurBuffer(totalCells);
    ApplyBlur(mapGrid, mapWidth, mapHeight, blurBuffer);
    ApplyBlur(mapGrid, mapWidth, mapHeight, blurBuffer);

    // FIX: Post-blur fail-safe override.
    // Forces a small 3x3 patch of grass directly under the player's spawn point
    // to guarantee they never clip into water or sand upon initialization.
    for (int sy = -1; sy <= 1; ++sy) {
        for (int sx = -1; sx <= 1; ++sx) {
            int targetX = std::clamp(cx + sx, 0, mapWidth - 1);
            int targetY = std::clamp(cy + sy, 0, mapHeight - 1);
            mapGrid[targetY * mapWidth + targetX] = 3; // Hard-set to Grass
        }
    }

    // ── Tree spawning ────────────────────────────────────────────────────
    trees.clear();

    constexpr int CELL_SIZE = 3;
    constexpr int CHECK_RADIUS = 2;

    const int gridW = (mapWidth  + CELL_SIZE - 1) / CELL_SIZE;
    const int gridH = (mapHeight + CELL_SIZE - 1) / CELL_SIZE;
    std::vector<bool> occupied(static_cast<size_t>(gridW) * gridH, false);

    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    std::uniform_int_distribution<int>   variant(0, 2);

    for (int y = 0; y < mapHeight; ++y) {
        for (int x = 0; x < mapWidth; ++x) {
            if (mapGrid[y * mapWidth + x] != 3) continue;

            // Prevent trees from spawning right on top of the dead-center player spawn
            if (std::abs(x - cx) <= 2 && std::abs(y - cy) <= 2) continue;

            constexpr int BORDER_MARGIN = 4;
            if (x < BORDER_MARGIN || x >= mapWidth  - BORDER_MARGIN ||
                y < BORDER_MARGIN || y >= mapHeight - BORDER_MARGIN) continue;

            float density = (fnlGetNoise2D(&densityNoise,
                                 static_cast<float>(x),
                                 static_cast<float>(y)) + 1.0f) * 0.5f;
            if (chance(gen) > 0.08f * density) continue;

            const int gx = x / CELL_SIZE;
            const int gy = y / CELL_SIZE;

            bool blocked = false;
            for (int ry = -CHECK_RADIUS; ry <= CHECK_RADIUS && !blocked; ++ry) {
                for (int rx = -CHECK_RADIUS; rx <= CHECK_RADIUS && !blocked; ++rx) {
                    const int nx = gx + rx;
                    const int ny = gy + ry;
                    if (nx < 0 || nx >= gridW || ny < 0 || ny >= gridH) continue;
                    if (occupied[static_cast<size_t>(ny) * gridW + nx])
                        blocked = true;
                }
            }
            if (blocked) continue;

            trees.emplace_back(
                static_cast<float>(x * 32),
                static_cast<float>(y * 32),
                variant(gen), true, 50, 1.0f
            );
            occupied[static_cast<size_t>(gy) * gridW + gx] = true;
        }
    }
}

void Engine::ApplyBlur(int* grid, int w, int h, std::vector<int>& buffer) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int counts[4] = {0, 0, 0, 0};
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    int val = GetTileValue(x + kx, y + ky, grid, w, h);
                    if (val >= 0 && val <= 3)
                        counts[val]++;
                }
            }

            int bestType = grid[y * w + x];
            int maxCount = 0;
            for (int i = 0; i < 4; ++i) {
                if (counts[i] > maxCount) {
                    maxCount = counts[i];
                    bestType = i;
                }
            }
            buffer[y * w + x] = bestType;
        }
    }
    std::ranges::copy(buffer, grid);
}

bool Engine::Initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init Failed: %s", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("Survive", 1980, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY);
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

    int w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    screenWidth = static_cast<float>(w);
    screenHeight = static_cast<float>(h);

    GenerateMap(128, 128);

    // initial player spawn
    playerX = (mapWidth  / 2) * 32.0f;
    playerY = (mapHeight / 2) * 32.0f;

    // load sounds
    if (!MIX_Init()) {
        SDL_Log("MIX_Init failed: %s", SDL_GetError());
    }

    mainMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (mainMixer) {
        footstepTracks[0] = MIX_CreateTrack(mainMixer);
        footstepTracks[1] = MIX_CreateTrack(mainMixer);

        // THE FIX: Create both weapon tracks!
        weaponFireTrack = MIX_CreateTrack(mainMixer);
        weaponMechTrack = MIX_CreateTrack(mainMixer);
        playerTrack = MIX_CreateTrack(mainMixer);
    }

    // load audio
    waterSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/water0.mp3", true);
    waterSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/water1.mp3", true);

    sandSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/sand0.mp3", true);
    sandSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/sand1.mp3", true);

    grassSteps[0] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/grass0.mp3", true);
    grassSteps[1] = MIX_LoadAudio(mainMixer, "Assets/Audio/Environment/grass1.mp3", true);

    punch_swing = MIX_LoadAudio(mainMixer, "Assets/Audio/Player/punch_swing.mp3", true);
    ak47_switch = MIX_LoadAudio(mainMixer, "Assets/Audio/Weapons/ak47_switch.mp3", true);
    ak47_fire = MIX_LoadAudio(mainMixer, "Assets/Audio/Weapons/ak47.mp3", true);
    ak47_reload = MIX_LoadAudio(mainMixer, "Assets/Audio/Weapons/ak47_reload.mp3", true);

    for (auto & enemyTrack : enemyTracks) {
        enemyTrack = MIX_CreateTrack(mainMixer);
    }

    spiderSteps[0] = MIX_LoadAudio(mainMixer, "Assets/NPC/Enemy/Sounds/Spider_step1.mp3", true);
    spiderSteps[1] = MIX_LoadAudio(mainMixer, "Assets/NPC/Enemy/Sounds/Spider_step2.mp3", true);

    // load textures

    activePaletteTex = LoadSVGToGPU("Assets/Player/Skins/Skin_default.svg", 64.0f, nullptr, nullptr);
    testPaletteTex = LoadSVGToGPU("Assets/Player/Skins/Skin_test1.svg", 64.0f, nullptr, nullptr);

    playerTex = LoadSVGToGPU("Assets/Player/Base/Body_mask.svg", 64.0f, &playerWidth, &playerHeight);
    armTex = LoadSVGToGPU("Assets/Player/Base/Arm_mask.svg", 64.0f, nullptr, nullptr);
    crosshairTex = LoadSVGToGPU("Assets/Player/Crosshair.svg", 32.0f, nullptr, nullptr);

    ak74Tex = LoadSVGToGPU("Assets/Player/Guns/Textures/AK74_mask.svg", 64.0f, nullptr, nullptr);

    tracer762Tex = LoadSVGToGPU("Assets/Player/Guns/Tracer/Tracer_762.svg", 64.0f, nullptr, nullptr);

    waterTex = LoadSVGToGPU("Assets/Environment/Water.svg", 64.0f, nullptr, nullptr);
    sandTex = LoadSVGToGPU("Assets/Environment/Sand.svg", 64.0f, nullptr, nullptr);
    grassTex = LoadSVGToGPU("Assets/Environment/Grass.svg", 64.0f, nullptr, nullptr);

    treeTex = LoadSVGToGPU("Assets/Environment/Trees_textures.svg", 64.0f, nullptr, nullptr);

    spiderTex = LoadSVGToGPU("Assets/NPC/Enemy/Textures/Spider_textures.svg", 128.0f, nullptr, nullptr);

    unitQuad = CreateUnitQuad();

    SDL_GPUSamplerCreateInfo samplerInfo = {};
    samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    nearestSampler = SDL_CreateGPUSampler(gpuDevice, &samplerInfo);

    SDL_GPUSamplerCreateInfo linearSamplerInfo = {};
    linearSamplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    linearSamplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    linearSamplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    linearSamplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linearSamplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    linearSampler = SDL_CreateGPUSampler(gpuDevice, &linearSamplerInfo);

    SDL_GPUSamplerCreateInfo paletteSamplerInfo = {};
    paletteSamplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    paletteSamplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    paletteSamplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    paletteSamplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    paletteSamplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    paletteSampler = SDL_CreateGPUSampler(gpuDevice, &paletteSamplerInfo);

    // hide os cursor
    SDL_HideCursor();

    if (!SetupPipeline()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Engine Error", "Failed to compile Shaders! Are the .spv files in the current directory?", nullptr);
        return false;
    }

    isRunning = true;
    return true;
}

bool Engine::DamageDestructibles(float hitX, float hitY, int damage, float radius) {
    for (auto& tree : trees) {
        if (!tree.active) continue;
        float dist = std::hypot(hitX - (tree.x + 64.0f), hitY - (tree.y + 64.0f));
        if (dist < radius) {
            tree.TakeDamage(damage);
            return true;
        }
    }
    return false;
}

void Engine::Run() {
    Player player(screenWidth / 2.0f, screenHeight / 2.0f);

    Weapon ak74(ak74Tex,
    64.0f, 64.0f,
                15.0f, 0.0f,
                0.0f, 0.0f, 1.0f / 6.0f, 1.0f / 5.0f,
                6, 5, 30,
                2.5f, 0.1f, 30, 15.25f);

    player.SetInventorySlot(0, &ak74);
    player.EquipSlot(0);

    Uint64 lastTime = SDL_GetTicksNS();

    while (isRunning) {
        int w, h;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        screenWidth = static_cast<float>(w);
        screenHeight = static_cast<float>(h);

        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        if (winW == 0) winW = 1;
        if (winH == 0) winH = 1;

        float targetLogicalHeight = 1080.0f;
        float zoom = screenHeight / targetLogicalHeight;
        float logicalWidth = screenWidth / zoom;
        float logicalHeight = screenHeight / zoom;

        Uint64 currentTime = SDL_GetTicksNS();
        float deltaTime = static_cast<float>(currentTime - lastTime) / 1e9f;
        lastTime = currentTime;

        ProcessInput(player);

        float rawMouseX, rawMouseY;
        SDL_GetMouseState(&rawMouseX, &rawMouseY);

        float physicalMouseX = (rawMouseX / static_cast<float>(winW)) * screenWidth;
        float physicalMouseY = (rawMouseY / static_cast<float>(winH)) * screenHeight;

        float logicalMouseX = physicalMouseX / zoom;
        float logicalMouseY = physicalMouseY / zoom;

        float cameraX = player.x - (logicalWidth / 2.0f);
        float cameraY = player.y - (logicalHeight / 2.0f);

        float worldMouseX = logicalMouseX + cameraX;
        float worldMouseY = logicalMouseY + cameraY;

        player.Update(deltaTime, SDL_GetKeyboardState(nullptr), worldMouseX, worldMouseY, mapGrid, mapWidth, mapHeight, *this);
        Render(player, logicalMouseX, logicalMouseY);

        for (auto it = activeTracers.begin(); it != activeTracers.end(); ) {
            it->life -= deltaTime;
            it->currentX += std::cos(it->angle) * it->speed * deltaTime;
            it->currentY += std::sin(it->angle) * it->speed * deltaTime;

            bool hitDestructible = DamageDestructibles(it->currentX, it->currentY, it->damage, 72.0f);
            bool bulletHit = false;

            for (auto & enemie : enemies) {
                if (enemie.active) {
                    float distToEnemy = std::hypot(enemie.x - it->currentX, enemie.y - it->currentY);

                    if (distToEnemy < 40.0f) {
                        float distTraveled = std::hypot(it->currentX - it->startX, it->currentY - it->startY);
                        int finalDamage = it->damage;

                        if (distTraveled > it->falloffStart) {
                            float falloffRange = it->falloffEnd - it->falloffStart;
                            float distancePastStart = distTraveled - it->falloffStart;

                            float falloffPct = distancePastStart / falloffRange;
                            if (falloffPct > 1.0f) falloffPct = 1.0f;

                            float damageLostPct = (1.0f - it->minDamagePct) * falloffPct;
                            finalDamage = static_cast<int>(static_cast<float>(it->damage) * (1.0f - damageLostPct));
                        }
                        enemie.hp -= finalDamage;
                        SDL_Log("Enemy hit!  HP: %d", enemie.hp);
                        if (enemie.hp <= 0) {
                            enemie.active = false;
                        }

                        bulletHit = true;
                        break;
                    }
                }
            }

            if (bulletHit || hitDestructible || it->life <= 0.0f) {
                it = activeTracers.erase(it);
            } else {
                ++it;
            }
        }

        if (player.justSwung) {
            if (playerTrack && punch_swing) {
                MIX_SetTrackGain(playerTrack, 1.0f);
                MIX_SetTrackAudio(playerTrack, punch_swing);
                MIX_PlayTrack(playerTrack, false);
            }

            float reach = 60.0f;
            float hitX = player.x + std::cos(player.armAngle) * reach;
            float hitY = player.y + std::sin(player.armAngle) * reach;

            for (auto & enemie : enemies) {
                if (enemie.active) {
                    float dist = std::hypot(enemie.x - hitX, enemie.y - hitY);

                    if (dist < 55.0f) {
                        enemie.hp -= 20;
                        SDL_Log("Enemy Punched! HP: %d", enemie.hp);

                        if (enemie.hp <= 0) {
                            enemie.active = false;
                            SDL_Log("Enemy Killed by Fists!");
                        }
                    }
                }
            }

            DamageDestructibles(hitX, hitY, 20, 55.0f);
            player.justSwung = false;
        }

        if (player.justEquipped) {
            if (weaponMechTrack && ak47_switch) {
                MIX_SetTrackGain(weaponMechTrack, 1.0f);
                MIX_SetTrackAudio(weaponMechTrack, ak47_switch);
                MIX_PlayTrack(weaponMechTrack, false);
            }
            player.justEquipped = false;
        }

        if (player.justReloaded) {
            if (weaponMechTrack && ak47_reload) {
                MIX_SetTrackGain(weaponMechTrack, 1.0f);
                MIX_SetTrackAudio(weaponMechTrack, ak47_reload);
                MIX_PlayTrack(weaponMechTrack, false);
            }
            player.justReloaded = false;
        }

        if (player.justFired) {
            if (weaponFireTrack && ak47_fire) {
                MIX_SetTrackGain(weaponFireTrack, 0.7f);
                MIX_SetTrackAudio(weaponFireTrack, ak47_fire);
                MIX_PlayTrack(weaponFireTrack, false);
            }
            player.justFired = false;
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

        if (globalEnemyStepCooldown > 0.0f) {
            globalEnemyStepCooldown -= deltaTime;
        }

        for (auto & enemie : enemies) {
            if (enemie.active) {
                enemie.Update(deltaTime, player, enemies, MAX_ENEMIES, trees);

                if (enemie.justStepped) {
                    if (globalEnemyStepCooldown <= 0.0f) {
                        MIX_Track* track = enemyTracks[currentEnemyTrackIndex];
                        if (track && spiderSteps[enemie.stepToggle]) {
                            MIX_SetTrackGain(track, 0.4f);
                            MIX_SetTrackAudio(track, spiderSteps[enemie.stepToggle]);
                            MIX_PlayTrack(track, false);
                            currentEnemyTrackIndex = (currentEnemyTrackIndex + 1) % MAX_ENEMY_TRACKS;
                            globalEnemyStepCooldown = 0.05f;
                        }
                    }
                    enemie.justStepped = false;
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
                player.Melee();
            }
        }

        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            screenWidth = static_cast<float>(event.window.data1);
            screenHeight = static_cast<float>(event.window.data2);
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
            if (event.key.scancode == SDL_SCANCODE_KP_3) {
                // Route execution context to our standalone manager class
                waveManager.TriggerNextWave(player, enemies, MAX_ENEMIES);
            }   
            if (event.key.scancode == SDL_SCANCODE_KP_4) {
                int enemiesToSpawn = 5;
                int spawnedCount = 0;

                for (auto & enemie : enemies) {
                    if (!enemie.active) {
                        float offsetX = static_cast<float>(spawnedCount) * 70.0f;
                        float offsetY = (spawnedCount % 2 == 0) ? 40.0f : -40.0f;

                        enemie.Spawn(player.x + 500.0f + offsetX, player.y + offsetY);

                        spawnedCount++;

                        if (spawnedCount >= enemiesToSpawn) {
                            break;
                        }
                    }
                }
                SDL_Log("Successfully spawned %d enemies!", spawnedCount);
            }
        }
    }
    Uint32 mouseState = SDL_GetMouseState(nullptr, nullptr);
    if (mouseState & SDL_BUTTON_LMASK) {
        float gunTipX, gunTipY;
        if (player.AttemptFire(gunTipX, gunTipY)) {
            player.justFired = true;
            Tracer newBullet{};
            newBullet.startX = gunTipX;
            newBullet.startY = gunTipY;
            newBullet.currentX = gunTipX;
            newBullet.currentY = gunTipY;

            newBullet.damage = static_cast<int>(player.equippedWeapon->damage);
            newBullet.falloffStart = player.equippedWeapon->falloffStart;
            newBullet.falloffEnd = player.equippedWeapon->falloffEnd;
            newBullet.minDamagePct = player.equippedWeapon->minDamagePct;

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
                     SDL_GPUTexture* texture, SDL_GPUTexture* palette,
                     const PushConstants& pc, SDL_GPUSampler* sampler) const {

    // CRASH PROTECTION: If textures aren't loaded, don't try to draw them
    if (!texture || !palette) return;

    SDL_GPUTextureSamplerBinding bindings[2];
    SDL_GPUSampler* activeSampler = (sampler != nullptr) ? sampler : nearestSampler;

    bindings[0].texture = texture;
    bindings[0].sampler = activeSampler;

    bindings[1].texture = palette;
    bindings[1].sampler = paletteSampler;

    SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, 2);

    SDL_PushGPUVertexUniformData(cmdBuf, 0, &pc, sizeof(PushConstants));
    SDL_PushGPUFragmentUniformData(cmdBuf, 0, &pc, sizeof(PushConstants));
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

    float targetLogicalHeight = 1080.0f;
    float zoom = screenHeight / targetLogicalHeight;
    float logicalWidth = screenWidth / zoom;
    float logicalHeight = screenHeight / zoom;

    SDL_GPUColorTargetInfo colorTarget = {};
    colorTarget.texture = swapchainTexture;
    colorTarget.clear_color = {0.1f, 0.15f, 0.1f, 1.0f};
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTarget, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

    SDL_GPUViewport viewport = { 0.0f, 0.0f, screenWidth, screenHeight, 0.0f, 1.0f };
    SDL_SetGPUViewport(renderPass, &viewport);
    SDL_Rect scissor = { 0, 0, static_cast<int>(screenWidth), static_cast<int>(screenHeight) };
    SDL_SetGPUScissor(renderPass, &scissor);

    SDL_GPUBufferBinding vertexBinding = { unitQuad, 0 };
    SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

    float cameraX = player.x - (logicalWidth / 2.0f);
    float cameraY = player.y - (logicalHeight / 2.0f);

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

                    if (screenX < -tileSize || screenX > logicalWidth ||
                        screenY < -tileSize || screenY > logicalHeight) {
                        continue;
                    }

                    int col = tileIndex % 4;
                    int row = tileIndex / 4;

                    float uvWidth = 0.25f;
                    float uvHeight = 0.25f;

                    float texSize = 1024.0f;
                    float halfPixel = 0.5f / texSize;

                    float uMin = (static_cast<float>(col) * uvWidth) + halfPixel;
                    float uMax = (static_cast<float>(col + 1) * uvWidth) - halfPixel;
                    float vMin = (static_cast<float>(row) * uvHeight) + halfPixel;
                    float vMax = (static_cast<float>(row + 1) * uvHeight) - halfPixel;

                    if (screenX < -tileSize || screenX > logicalWidth ||
                        screenY < -tileSize || screenY > logicalHeight) {
                        continue;
                        }

                    PushConstants tileData = {
                        logicalWidth, logicalHeight,
                        screenX + (tileSize / 2.0f), screenY + (tileSize / 2.0f),
                        tileSize, tileSize,
                        1.0f, 0.0f,
                        uMin, vMin, uMax - uMin, vMax - vMin,
            {1.0f, 1.0f, 1.0f, -1.0f}};
                    DrawQuad(cmdBuf, renderPass, layerTexture, layerTexture, tileData, nearestSampler);
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

    float endX = cameraX + logicalWidth;
    float endY = cameraY + logicalHeight;

    // draw vertical chunk lines
    int startCol = static_cast<int>(std::floor(startX / chunkSize));
    int endCol = static_cast<int>(std::ceil(endX / chunkSize));


    for (int i = startCol; i <= endCol; ++i) {
        float x = static_cast<float>(i) * chunkSize;
        float screenX = x - cameraX;

        PushConstants verticalLine = {
            logicalWidth, logicalHeight,
            screenX, logicalHeight / 2.0f,
            lineThickness, logicalHeight,
            1.0f, 0.0f,

            0.5f, 0.5f, 0.0f, 0.0f,

            {0.5f, 0.5f, 0.5f, -1.0f}
        };
        DrawQuad(cmdBuf, renderPass, crosshairTex, crosshairTex, verticalLine, linearSampler);
    }

    // draw horizontal chunk lines
    int startRow = static_cast<int>(std::floor(startY / chunkSize));
    int endRow = static_cast<int>(std::ceil(endY / chunkSize));

    for (int i = startRow; i <= endRow; ++i) {
        float y = static_cast<float>(i) * chunkSize;
        float screenY = y - cameraY;

        PushConstants horizontalLine = {
            logicalWidth, logicalHeight,
            logicalWidth / 2.0f, screenY,
            logicalWidth, lineThickness,
            1.0f, 0.0f,

            0.5f, 0.5f, 0.0f, 0.0f,

            {0.5f, 0.5f, 0.5f, -1.0f}
        };
        DrawQuad(cmdBuf, renderPass, crosshairTex, crosshairTex, horizontalLine, linearSampler);
    }

    float drawAngle = player.armAngle + 1.570796f;
    // draw call 1: body
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
        logicalWidth, logicalHeight,
        player.x - cameraX, player.y - cameraY,
        64.0f, 64.0f,
        std::cos(drawAngle), std::sin(drawAngle),
        bodyUvX, bodyUvY, bodyUvW, bodyUvH,
        { currentArmorColor[0], currentArmorColor[1], currentArmorColor[2], currentArmorColor[3] }
    };

    DrawQuad(cmdBuf, renderPass, playerTex, currentSkinTex, bodyData, nearestSampler);

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

        if (player.equippedWeapon->isReloading) {
            float progress = player.equippedWeapon->currentReloadTimer / player.equippedWeapon->reloadTime;
            animFrame = static_cast<int>(progress * static_cast<float>(totalFrames));

            if (animFrame >= totalFrames) animFrame = totalFrames - 1;
        }
        // 2. SHOOTING: Apply the physical recoil kick, but DO NOT animate the texture!
        else if (player.isAttacking) {
            animFrame = 0; // <-- FIX: Force the texture to stay on the Idle frame!

            // Apply the physical push-back using the player's melee frame timer
            if (player.currentFrame == 1) recoil = -5.0f;
            if (player.currentFrame == 2) recoil = -2.5f;
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
            logicalWidth, logicalHeight,
            gunX - cameraX, gunY - cameraY,
            player.equippedWeapon->width, player.equippedWeapon->height,
            std::cos(drawAngle), std::sin(drawAngle),
            currentUvX, currentUvY, frameWidth, frameHeight,
            { currentArmorColor[0], currentArmorColor[1], currentArmorColor[2], 1.0f }
        };

        DrawQuad(cmdBuf, renderPass, player.equippedWeapon->texture, currentSkinTex, weaponData, linearSampler);
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
            logicalWidth, logicalHeight,
            handX - cameraX, handY - cameraY,
            64.0f, 64.0f,
            std::cos(drawAngle), std::sin(drawAngle),
            currentArmUvX, 0.0f, armUvW, armUvH,
            { currentArmorColor[0], currentArmorColor[1], currentArmorColor[2], 1.0f }
        };

        DrawQuad(cmdBuf, renderPass, armTex, currentSkinTex, armData, nearestSampler);
    }

    // draw call 2.5: tracers
    for (const auto& tracer : activeTracers) {
        PushConstants tracerData = {
            logicalWidth, logicalHeight,
            tracer.currentX - cameraX, tracer.currentY - cameraY,
            100.0f, 8.0f,
            std::cos(tracer.angle), std::sin(tracer.angle),
            0.0f, 0.0f, 1.0f, 1.0f,
            {1.0f, 1.0f, 1.0f, -1.0f}
        };
        DrawQuad(cmdBuf, renderPass, tracer762Tex, tracer762Tex, tracerData, linearSampler);
    }

    //draw call 3: GIANT ENEMY SPIDER TU TU TUTUTUTTUTUTUTU
    for (const auto & enemie : enemies) {
        if (!enemie.active) continue;

        float frameW = 1.0f / 4.0f;
        float frameH = 1.0f / 3.0f;

        int col = enemie.currentFrame % 4;
        int row = enemie.currentFrame / 4;

        float angle = std::atan2(player.y - enemie.y, player.x - enemie.x) + 1.570796f;

        PushConstants enemyData = {
            logicalWidth, logicalHeight,
            enemie.x - cameraX, enemie.y - cameraY,
            96.0f, 96.0f,
            std::cos(angle), std::sin(angle),
            static_cast<float>(col) * frameW, static_cast<float>(row) * frameH, frameW, frameH,
            {1.0f, 1.0f, 1.0f, -1.0f}
        };

        DrawQuad(cmdBuf, renderPass, spiderTex, spiderTex, enemyData, linearSampler);
    }

    // draw call 4: crosshair
    PushConstants crosshairData = {
        logicalWidth, logicalHeight,
        mouseX, mouseY,
        32.0f, 32.0f,
        1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        {1.0f, 1.0f, 1.0f, -1.0f}
    };
    DrawQuad(cmdBuf, renderPass, crosshairTex, crosshairTex, crosshairData, linearSampler);

    // draw call 5: trees
    for (const auto& tree : trees) {
        if (!tree.active) continue;

        // Tree texture is 3x1 (3 variants)
        float frameW = 1.0f / 3.0f;
        float frameH = 1.0f;

        // Calculate UV based on variant
        float uMin = static_cast<float>(tree.variant) * frameW;

        // Use tree position (adjust offset if needed for centering)
        PushConstants treeData = {
            logicalWidth, logicalHeight,
            tree.x - cameraX + 64.0f, tree.y - cameraY + 64.0f,
            256.0f, 256.0f,
            1.0f, 0.0f, // Rotation
            uMin, 0.0f, frameW, frameH,
            {1.0f, 1.0f, 1.0f, 1.0f}
        };

        DrawQuad(cmdBuf, renderPass, treeTex, treeTex, treeData, linearSampler);
    }

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
        if (nearestSampler) SDL_ReleaseGPUSampler(gpuDevice, nearestSampler);
        if (linearSampler) SDL_ReleaseGPUSampler(gpuDevice, linearSampler);
        if (unitQuad) SDL_ReleaseGPUBuffer(gpuDevice, unitQuad);

        if (playerTex) SDL_ReleaseGPUTexture(gpuDevice, playerTex);
        if (crosshairTex) SDL_ReleaseGPUTexture(gpuDevice, crosshairTex);
        if (armTex) SDL_ReleaseGPUTexture(gpuDevice, armTex);

        if (ak74Tex) SDL_ReleaseGPUTexture(gpuDevice, ak74Tex);

        if (tracer762Tex) SDL_ReleaseGPUTexture(gpuDevice, tracer762Tex);

        if (waterTex) SDL_ReleaseGPUTexture(gpuDevice, waterTex);
        if (grassTex) SDL_ReleaseGPUTexture(gpuDevice, grassTex);
        if (sandTex) SDL_ReleaseGPUTexture(gpuDevice, sandTex);

        if (treeTex) SDL_ReleaseGPUTexture(gpuDevice, treeTex);

        for (int i=0; i<2; i++) {
            if (waterSteps[i]) MIX_DestroyAudio(waterSteps[i]);
            if (sandSteps[i]) MIX_DestroyAudio(sandSteps[i]);
            if (grassSteps[i]) MIX_DestroyAudio(grassSteps[i]);
        }
        if (punch_swing) MIX_DestroyAudio(punch_swing);
        if (ak47_switch) MIX_DestroyAudio(ak47_switch);
        if (ak47_fire) MIX_DestroyAudio(ak47_fire);
        if (ak47_reload) MIX_DestroyAudio(ak47_reload);
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

    colorTargetDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTargetDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

    colorTargetDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    colorTargetDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;

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
    // 1. Initialize resvg options
    resvg_options* opt = resvg_options_create();
    resvg_options_set_shape_rendering_mode(opt, RESVG_SHAPE_RENDERING_CRISP_EDGES);
    resvg_render_tree* tree = nullptr;

    // 2. Parse the SVG file
    int err = resvg_parse_tree_from_file(filepath, opt, &tree);
    resvg_options_destroy(opt); // We can destroy options immediately after parsing

    if (err != RESVG_OK || !tree) {
        SDL_Log("CRITICAL: resvg failed to load SVG file: %s (Error Code: %d)", filepath, err);
        return nullptr;
    }

    // 3. Get document dimensions
    resvg_size size = resvg_get_image_size(tree);
    double docW = size.width;
    double docH = size.height;

    if (docW <= 0.0 || docH <= 0.0) {
        docW = 100.0;
        docH = 100.0;
    }

    // 4 Vulkan Scaling
    double targetWidth = 2048.0;
    double uniformScale = targetWidth / docW;

    uint32_t finalWidth = (static_cast<uint32_t>(docW * uniformScale) / 64) * 64;

    uniformScale = static_cast<double>(finalWidth) / docW;

    auto finalHeight = static_cast<uint32_t>(docH * uniformScale);

    double actualScaleX = uniformScale;
    double actualScaleY = uniformScale;

    // 5. Create GPU Texture
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
        resvg_tree_destroy(tree);
        return nullptr;
    }

    // 6. Create GPU Transfer Buffer
    Uint32 imageSize = finalWidth * finalHeight * 4;
    SDL_GPUTransferBufferCreateInfo transferInfo = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = imageSize
    };

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);
    if (!transferBuffer) {
        SDL_Log("CRITICAL: Failed to create transfer buffer for %s", filepath);
        SDL_ReleaseGPUTexture(gpuDevice, gpuTexture);
        resvg_tree_destroy(tree);
        return nullptr;
    }

    // 7. Map memory and let resvg render directly into the GPU staging buffer (Zero-copy!)
    if (auto* map = static_cast<char*>(SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false))) {

        // CRITICAL: Initialize memory to zero to prevent transparent backgrounds from showing garbage data
        std::memset(map, 0, imageSize);

        // Build the transformation matrix to scale the vectors
        resvg_transform transform = {
            static_cast<float>(actualScaleX), 0.0f, // a, b
            0.0f, static_cast<float>(actualScaleY), // c, d
            0.0f, 0.0f                              // e, f
        };

        // Render straight to mapped memory (Outputs perfect RGBA natively, no swizzling needed)
        resvg_render(tree, transform, finalWidth, finalHeight, map);
        SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);
    }

    // Clean up the SVG DOM tree, we don't need it anymore
    resvg_tree_destroy(tree);

    // 8. Submit to GPU
    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(gpuDevice);
    if (!cmdBuf) {
        SDL_ReleaseGPUTexture(gpuDevice, gpuTexture);
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
    SDL_SubmitGPUCommandBuffer(cmdBuf);

    // Stall CPU until upload finishes. (In the future, consider a fence for asynchronous loading).
    SDL_WaitForGPUIdle(gpuDevice);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);

    return gpuTexture;
}