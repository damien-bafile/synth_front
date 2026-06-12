cmake_minimum_required(VERSION 3.16)
project(synth_front LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/deps")

# MSYS2/MinGW reports as "MSYS" but we want WIN32 behavior
if(CMAKE_SYSTEM_NAME STREQUAL "MSYS")
  set(CMAKE_SYSTEM_NAME Windows CACHE STRING "" FORCE)
  set(WIN32 TRUE CACHE BOOL "" FORCE)
  set(UNIX FALSE CACHE BOOL "" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.8
  GIT_SHALLOW TRUE
)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_DIRECTX OFF CACHE BOOL "" FORCE)
set(SDL_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(
  synth_ui_lib
  GIT_REPOSITORY https://github.com/damien-bafile/synth_ui_lib.git
  GIT_TAG main
)
FetchContent_MakeAvailable(synth_ui_lib)

if(WIN32)
  set(ONLY_LIBS ON CACHE BOOL "" FORCE)
  set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    glew
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG glew-cmake-2.3.1
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(glew)
endif()

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.c")
list(FILTER SOURCES EXCLUDE REGEX "midi_input_(coremidi|alsa|win32)\\.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src")
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Werror -Wall -Wextra -Wpedantic)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 synth_ui_lib_static)

if(APPLE)
  target_sources(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src/midi/midi_input_coremidi.cpp")
  target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
  find_library(OPENGL_LIBRARY OpenGL REQUIRED)
  find_library(COREMIDI_LIBRARY CoreMIDI REQUIRED)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OPENGL_LIBRARY} ${COREMIDI_LIBRARY} ${COREFOUNDATION_LIBRARY})
elseif(UNIX)
  target_sources(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src/midi/midi_input_alsa.cpp")
  find_package(OpenGL REQUIRED)
  find_library(ALSA_LIBRARY asound REQUIRED)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL ${ALSA_LIBRARY})
elseif(WIN32)
  target_sources(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src/midi/midi_input_win32.cpp")
  target_link_libraries(${PROJECT_NAME} PRIVATE
    winmm opengl32 setupapi libglew_static)
endif()
