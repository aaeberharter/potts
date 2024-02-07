cmake_minimum_required(VERSION 3.16)
project(PottsApp)

set(CMAKE_CXX_STANDARD 20)
include(CPM.cmake)

# some fork of imgui which has cmake integration and comes with SDL
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
CPMAddPackage("gh:Pesc0/imgui-cmake#master")

# GLEW
CPMAddPackage("gh:Perlmint/glew-cmake#master")

# HLSLPP - header only - not a cmake project
CPMAddPackage("gh:redorav/hlslpp#master")


set(CUSTOM_SOURCES AppPotts.cpp)

add_executable(PottsApp AppSDL.cpp AppSDL.h VirtualInput.cpp VirtualInput.h global.h ${CUSTOM_SOURCES})

target_include_directories(PottsApp
        PUBLIC
        $<BUILD_INTERFACE:${hlslpp_SOURCE_DIR}/include>
        PRIVATE
        $<TARGET_PROPERTY:IMGUI,INCLUDE_DIRECTORIES>
)
target_sources(PottsApp PRIVATE $<TARGET_PROPERTY:IMGUI,SOURCES>)
target_link_libraries(PottsApp libglew_static SDL3-static ${CMAKE_DL_LIBS})