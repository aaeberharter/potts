#ifndef CPP_SDL2_GLOBAL_H
#define CPP_SDL2_GLOBAL_H

#include <SDL.h>
#undef main
#include <cmath>
#include <random>
#include <hlsl++.h>
using namespace hlslpp;

#include "VirtualInput.h"

struct State;
struct Settings;

struct VirtualInputs
{
    VirtualAxis zoom;
    VirtualAxis up_down;
    VirtualAxis right_left;
    VirtualToggle drag;
};

struct GlobalData
{
    struct MouseData
    {
        float2 pos;
        int2 wheel;
        uint8_t button[5];
        void set_buttons(uint32_t state)
        {
            button[0] = state & SDL_BUTTON(SDL_BUTTON_LEFT);
            button[1] = state & SDL_BUTTON(SDL_BUTTON_MIDDLE);
            button[2] = state & SDL_BUTTON(SDL_BUTTON_RIGHT);
            button[3] = state & SDL_BUTTON(SDL_BUTTON_X1);
            button[4] = state & SDL_BUTTON(SDL_BUTTON_X2);
        }
    };

    bool running;

    int2 wsize; // Window width and height
    float2 fwsize; // width, height as float

    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;

    uint8_t* keyboard_state_current = nullptr;
    uint8_t* keyboard_state_previous = nullptr;

    MouseData* mouse_state_current = nullptr;
    MouseData* mouse_state_previous = nullptr;

    VirtualInputs virtual_inputs;

    State* state;


    const int POTTS_Q_MAX = 10;
    const int POTTS_X_MIN = 64;
    const int POTTS_Y_MIN = 64;
    const int POTTS_X_MAX = 16384;
    const int POTTS_Y_MAX = 16384;

    // seed = 1905
	std::default_random_engine rgen = std::default_random_engine(1905);
};

// Defined in AppSDL.cpp
extern GlobalData G;

#endif //CPP_SDL2_GLOBAL_H
