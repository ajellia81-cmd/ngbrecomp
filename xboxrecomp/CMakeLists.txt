cmake_minimum_required(VERSION 3.20)
project(xboxrecomp VERSION 0.9.0 LANGUAGES C)

# Xbox Static Recompilation Runtime Library
# Provides link-time libraries for statically recompiled Xbox games.
# Pattern: extract emulator subsystems (xemu) into reusable static libraries.
#
# Libraries:
#   xbox_kernel    - Xbox kernel replacement layer (147 imports → Win32)
#   xbox_d3d8      - D3D8 → D3D11 graphics compatibility
#   xbox_dsound    - DirectSound → software mixer compatibility
#   xbox_input     - Xbox gamepad → XInput compatibility
#   xbox_apu       - MCPX APU audio emulation (extracted from xemu)
#   xbox_nv2a      - NV2A GPU emulation (extracted from xemu)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common definitions for all targets
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Option to build examples

# ── Runtime Libraries ─────────────────────────────────────────

# Windows builds can use the host backends the Linux build uses -- SDL2 audio
# and input, no Media Foundation, XInput or XAudio2 -- so a port that runs on
# both behaves the same on both (keyboard, mouse, any SDL pad).
option(XBOXRECOMP_SDL_BACKENDS "Windows: SDL2 audio/input instead of XAudio2/XInput/Media Foundation" OFF)
if(WIN32 AND XBOXRECOMP_SDL_BACKENDS)
    add_compile_definitions(XBOX_SDL_BACKENDS)
endif()
# Windows builds can also run guest threads on one virtual CPU, as POSIX
# builds do (src/platform/vcpu.h): titles that need the console's scheduling
# -- a thread made ready above the running one runs at once -- hang or spin
# without it.
option(XBOXRECOMP_WIN_VCPU "Windows: guest threads on one virtual CPU (platform/vcpu.h)" OFF)
option(XBOXRECOMP_NV2A_VK "Build the NV2A Vulkan renderer (from xemu)" OFF)

# Platform: Win32/NT type vocabulary + POSIX implementation of generic
# Win32 host primitives. Carries no Xbox semantics; provides the foundation
# the kernel HLE and the graphics/audio/input layers build on.
add_subdirectory(src/platform)

# Core: Xbox kernel replacement (memory layout, file I/O, threading, sync)
add_subdirectory(src/kernel)

# Graphics: D3D8 → D3D11 compatibility layer
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(NOT XBOXRECOMP_NV2A_VK)
        message(FATAL_ERROR "iOS requires the Vulkan renderer; the legacy desktop OpenGL 3.3 backend is unavailable")
    endif()
    # This title draws through src/d3d/vk and NV2A, not the separate legacy
    # D3D8/OpenGL library. Keep its header vocabulary for shared video code;
    # an accidental use of a legacy D3D entry point must fail at link time.
    add_library(xbox_d3d8 INTERFACE)
    target_include_directories(xbox_d3d8 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/d3d)
    target_link_libraries(xbox_d3d8 INTERFACE platform)
else()
    add_subdirectory(src/d3d)
endif()

# Audio: DirectSound → software mixer compatibility
add_subdirectory(src/audio)

# Audio: MCPX APU hardware emulation (voice processor, DSP)
add_subdirectory(src/apu)

# GPU: NV2A hardware emulation (register handlers, MMIO, push buffer)
add_subdirectory(src/nv2a)

# USB: MCPX OHCI host controllers. A title's XAPI is statically linked and
# drives these registers directly, so a pad only exists if they do.
add_subdirectory(src/usb)

# Input: Xbox gamepad → XInput
add_subdirectory(src/input)

# Shared FMV player (Media Foundation -> D3D8 texture)
add_subdirectory(src/video)

# GPU: xemu's NV2A model with its Vulkan renderer, for titles whose own D3D
# drives the GPU through pushbuffers. Opt-in; Linux only for now.
if(XBOXRECOMP_NV2A_VK)
    # VMA is C++; enabled here so the consuming project links as C++ too.
    enable_language(CXX)
    add_subdirectory(src/nv2a_vk)
endif()

# ── Umbrella target ───────────────────────────────────────────

# Convenience target that links all runtime libraries
add_library(xboxrecomp INTERFACE)
target_link_libraries(xboxrecomp INTERFACE
    xbox_kernel
    xbox_d3d8
    xbox_dsound
    xbox_input
    xbox_usb
    xbox_apu
    xbox_nv2a
    xbox_video
)

