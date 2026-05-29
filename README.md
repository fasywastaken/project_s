# Project S

A high-performance, procedurally generated 2D top-down survival shooter built from scratch in C++ using the SDL3 ecosystem and modern GPU hardware acceleration.

The game features a randomly generated island driven by multi-layered domain-warped noise, a fully decoupled scalable wave system, and runtime vector graphic rasterization.

## Core Features

* **Domain-Warped OpenSimplex2 Noise**: Natively maps smooth, organic landmasses and continuous coastlines using `FastNoiseLite`.
* **Island Cubic Falloff**: Automatically bounds procedural terrain layout structure into a cohesive central island.
* **Cellular Blending Passes**: Runs a dual-pass local neighborhood analysis to smoothly blend tile arrays into scannable layers (Water, Sand, Grass).
* **Poisson-Style Tree Grid**: Utilizes a dynamic grid cell occupancy system that keeps trees naturally distributed while completely preventing item overlaps.
* **Flocking AI Swarms**: Enemies apply proximity-based separation forces (`1.0f - (dist / pushRadius)`) to prevent sprite stacking and overlap.
* **Context-Aware Footsteps**: Interrogates position vectors on the underlying tile map to dynamically shift player step audio between water, sand, and grass.

## Tech Stack & Dependencies

* **Framework**: SDL3 core library modules (Video, Audio, and modern SDL_GPU sub-elements).
* **Graphics API**: Modern SDL_GPU subsystem natively requesting Vulkan / SPIR-V bytecode pipelines with absolute hardware fallback handling.
* **Vector Graphics Pipeline**: `resvg` API parses and renders SVGs directly into mapped GPU staging buffers for zero-copy runtime rasterization.
* **Procedural Engine**: `FastNoiseLite.h` for multi-octave fractal configuration models.
* **Data Serialization**: `json.hpp` for structure configuration file processing.

## Controls & Keybinds

| Key                       | Action                                                                               |
|:--------------------------|:-------------------------------------------------------------------------------------|
| **`W` / `A` / `S` / `D`** | Player movement (Automatically normalized diagonally to prevent speed exploitation). |
| **`Mouse Move`**          | Direct weapon aiming vectors and orientation tracking.                               |
| **`Left Mouse Click`**    | Continuous firearm discharge or unarmed melee combo pacing.                          |
| **`R`**                   | Manually trigger weapon magazine replenishment state machine.                        |
| **`1` / `2` / `3`**       | Hotkey slots to toggle equipment arrays (Primary, Secondary, Unarmed Melee).         |
| **`Numpad 3`**            | Triggers the next incoming scalable enemy wave.                                      |
| **`Numpad 4`**            | Debug spawn 5 default spider enemies directly in front of the player position.       |
| **`Numpad 7`**            | Live switch between 8 distinct character uniform gear layouts.                       |
| **`Numpad 8`**            | Cycle player base character skin palettes instantly.                                 |
| **`Numpad 9`**            | Toggle player body armor rendering tiers (Levels 0 through 3).                       |
