cmake_minimum_required(VERSION 3.16)
project(soft-rasterizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.10
  GIT_SHALLOW TRUE
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

add_executable(soft_rasterizer
  src/main.cpp
  src/math/Mat4.cpp
  src/core/Camera.cpp
  src/core/Mesh.cpp
  src/render/Rasterizer.cpp
  src/render/Lighting.cpp
)

target_include_directories(soft_rasterizer PRIVATE src)
target_link_libraries(soft_rasterizer PRIVATE SDL2::SDL2main SDL2::SDL2)

if(MSVC)
  target_compile_options(soft_rasterizer PRIVATE /W4 /utf-8)
else()
  target_compile_options(soft_rasterizer PRIVATE -Wall -Wextra)
endif()
