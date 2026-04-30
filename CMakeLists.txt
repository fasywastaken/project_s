cmake_minimum_required(VERSION 3.20)
project(project_s)

set(CMAKE_CXX_STANDARD 20)

#SDL3
include(FetchContent)
FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        main
)
FetchContent_MakeAvailable(SDL3)

set(SDLMIXER_MP3 ON CACHE BOOL "Enable MP3 support" FORCE)

#preventing red text in console
set(SDLMIXER_MOD OFF CACHE BOOL "Disable MOD support" FORCE)
set(SDLMIXER_MIDI OFF CACHE BOOL "Disable MIDI support" FORCE)
set(SDLMIXER_FLAC OFF CACHE BOOL "Disable FLAC support" FORCE)
set(SDLMIXER_VORBIS OFF CACHE BOOL "Disable VORBIS support" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "Disable WAVPACK support" FORCE)

FetchContent_Declare(
        SDL3_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        GIT_TAG        main
)
FetchContent_MakeAvailable(SDL3_mixer)


add_executable(${PROJECT_NAME} main.cpp
        Engine.cpp
        Engine.h
        Player.cpp
        Player.h
        Weapon.cpp
        Weapon.h)

target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL3::SDL3
        SDL3_mixer::SDL3_mixer
)

#Asset Folder
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets"
)

#Shaders
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
            COMMENT "Compiling ${FILE_NAME} to SPIR-V..."
    )
endforeach ()