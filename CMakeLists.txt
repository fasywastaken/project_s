cmake_minimum_required(VERSION 3.20)
project(project_s)

set(CMAKE_CXX_STANDARD 20)

# sdl3
include(FetchContent)
FetchContent_Declare(
        SDL3
        URL https://github.com/libsdl-org/SDL/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(SDL3)

set(SDLMIXER_MP3 ON CACHE BOOL "Enable MP3 support" FORCE)

# preventing red text in console
set(SDLMIXER_MOD OFF CACHE BOOL "Disable MOD support" FORCE)
set(SDLMIXER_MIDI OFF CACHE BOOL "Disable MIDI support" FORCE)
set(SDLMIXER_FLAC OFF CACHE BOOL "Disable FLAC support" FORCE)
set(SDLMIXER_VORBIS OFF CACHE BOOL "Disable VORBIS support" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "Disable WAVPACK support" FORCE)

FetchContent_Declare(
        SDL3_mixer
        URL https://github.com/libsdl-org/SDL_mixer/archive/refs/heads/main.zip
)
FetchContent_MakeAvailable(SDL3_mixer)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libs" FORCE)
FetchContent_Declare(
        lunasvg
        URL https://github.com/sammycage/lunasvg/archive/refs/tags/v3.0.0.zip
)
FetchContent_MakeAvailable(lunasvg)

add_executable(${PROJECT_NAME} src/main.cpp
        src/Engine.cpp
        src/Engine.h
        src/Player.cpp
        src/Player.h
        src/Weapon.cpp
        src/Weapon.h)

target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL3::SDL3
        SDL3_mixer::SDL3_mixer
        lunasvg
)

# asset folder
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets"
)

# shaders
find_program(GLSLC glslc REQUIRED)
set(SHADER_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/sprite.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/sprite.frag"
)

foreach(SHADER IN LISTS SHADER_SOURCES)
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPV_FILE "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${FILE_NAME}.spv")

    add_custom_command(
            TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${GLSLC} ${SHADER} -o ${SPV_FILE}
    )
endforeach ()