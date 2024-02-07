#include <iostream>
#include <iomanip>
#include <chrono>
#include <ratio>
#include <thread>
//Using SDL and standard IO
#include <SDL.h>
#include <SDL_syswm.h>
#undef main

#include "global.h"
#include "AppSDL.h"

//Screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

// Some aliases to retain a minimum of sanity
using Timepoint = std::chrono::high_resolution_clock::time_point;
using Duration = std::chrono::high_resolution_clock::duration;
constexpr inline float duration_count(Duration dur){return std::chrono::duration_cast<std::chrono::duration<float>>(dur).count();}
inline Timepoint now(){return std::chrono::high_resolution_clock::now();}

Timepoint current_timepoint = now();
constexpr Duration target_frame_duration = std::chrono::milliseconds(20);
constexpr float dtf = duration_count(target_frame_duration);


GlobalData G={0};

bool init_window()
{
    G.running = true;
    G.wsize.x = SCREEN_WIDTH;
    G.wsize.y = SCREEN_HEIGHT;
    G.fwsize = float2(G.wsize);

    if( SDL_Init( SDL_INIT_VIDEO) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        return false;
    }
    //G.window = SDL_CreateWindow( "AppSDL", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT,
    //                             SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    G.window = SDL_CreateWindow( "AppSDL", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if( !G.window )
    {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        return false;
    }

    // Collect information about the window from SDL
    //SDL_SysWMinfo wmi;
    //SDL_VERSION(&wmi.version);
    //if (!SDL_GetWindowWMInfo(G.window, &wmi)) {
    //    return 1;
    //}

    // OpenGL + Imgui
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    G.gl_context = SDL_GL_CreateContext(G.window);
    glewInit();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    //ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(G.window, G.gl_context);
    ImGui_ImplOpenGL3_Init();


    int numkeys;
    const uint8_t* kstate = SDL_GetKeyboardState(&numkeys);
    G.keyboard_state_current = new uint8_t[numkeys];
    G.keyboard_state_previous = new uint8_t[numkeys];
    std::copy_n(kstate, numkeys, G.keyboard_state_current);

    G.mouse_state_current = new GlobalData::MouseData();
    G.mouse_state_previous = new GlobalData::MouseData();

    return true;
}

bool handle_events()
{
    SDL_PumpEvents();
    int numkeys;
    const uint8_t* kstate = SDL_GetKeyboardState(&numkeys);
    uint8_t* tmp = G.keyboard_state_current;
    G.keyboard_state_current = G.keyboard_state_previous;
    G.keyboard_state_previous = tmp;
    std::copy_n(kstate, numkeys, G.keyboard_state_current);

    float mouse_x, mouse_y;
    uint32_t mouse_button_state = SDL_GetMouseState(&mouse_x, &mouse_y);
    std::swap(G.mouse_state_current, G.mouse_state_previous);
    G.mouse_state_current->pos = int2(mouse_x, mouse_y);
    G.mouse_state_current->set_buttons(mouse_button_state);
    // wx and wy are populated by event, however we do not get an extra event which
    // would reset those values, we have to do it manually
    G.mouse_state_current->wheel = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        switch (event.type)
        {
        case SDL_EVENT_MOUSE_WHEEL:
            // No polling possible here?
            G.mouse_state_current->wheel = int2(event.wheel.x, event.wheel.y);
            break;

        case SDL_EVENT_QUIT:
            return false;

        case SDL_EVENT_WINDOW_RESIZED:
            // This is triggered after resize is complete
            G.wsize.x = event.window.data1;
            G.wsize.y = event.window.data2;
            G.fwsize = float2(G.wsize);
            break;

            default:
                break;
        }
    }

    VirtualInputs &vi = G.virtual_inputs;
    vi.right_left.update();
    vi.up_down.update();
    vi.zoom.update();
    vi.drag.update();
    return true;
}

bool destroy_window()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    //Destroy window
    SDL_DestroyWindow( G.window );
    G.window = nullptr;

    //Quit SDL subsystems
    SDL_Quit();
    return true;
}

int main()
{
    init_window();

    init_app();

    // We have to separate the slow rendering of the spins with GUI rendering and event handling
    float time_without_slow_render = 0.0f;
    float time_needed_for_slow_render = 0.0f;
    while ( G.running )
    {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Beginning
        Timepoint new_timepoint = now();
        Duration frame_duration = new_timepoint - current_timepoint;
        current_timepoint = new_timepoint;

        G.running = handle_events();

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Integrate state should try to not take longer time than 'dtf'
        // TODO make this better instead of just 0.5*dtf is wasting a lot of utilization
        integrate_state(G.state, 0.5*dtf);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Rendering
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame(/*G.window*/);
        ImGui::NewFrame();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        bool we_have_done_slow_render_this_frame = true;
        /*
        if (time_without_slow_render >= time_needed_for_slow_render)
        {
            we_have_done_slow_render_this_frame = true;
            Timepoint slow_render_start = now();
            render_state(G.state, false);
            time_needed_for_slow_render = duration_count(now() - slow_render_start);
            time_without_slow_render = 0.0;
        }
        else
        {
            we_have_done_slow_render_this_frame = false;
            render_state(G.state, true);
        }
        */


        render_state(G.state, true);
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // GUI

        render_gui(G.state);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(G.window);

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // End
        Duration current_frame_duration = now() - new_timepoint;
        if (current_frame_duration < target_frame_duration)
        {
            if (!we_have_done_slow_render_this_frame)
                time_without_slow_render += duration_count(target_frame_duration - current_frame_duration);
            std::this_thread::sleep_for(target_frame_duration - current_frame_duration);
        }
    }

    destroy_app();

    destroy_window();
}