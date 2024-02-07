//
// Created by alex on 09/04/2022.
//
#include "global.h"
#include "AppSDL.h"

#include <random>
#include <string>
#include <vector>
#include <array>
#include <iostream>
#include <fstream>

//#include "base_resample.h"

static int DEBUG_LINE_NUMBER = 0;

//#define LINE
//#define LINE std::cout << __LINE__ << std::endl;
#define LINE DEBUG_LINE_NUMBER = __LINE__;

struct PhysicalSettings
{
    // Settings which need recompution of some things
    int N_X, N_Y;
    int Q; // number of spins per site
    float temperature;
    float interaction; // J
    std::vector<float> field; // 1d Array with size Q, local field per spin state
};

struct RenderState
{
    // New GPU based potts update - not implemented yet
    GLuint spin_texture_0;
    GLuint spin_texture_1;


    GLuint vao_dummy;
    GLuint framebuffer_x_averaged;
    GLuint texture_x_averaged;
    GLuint texture_spin;

    GLuint shader_vert;
    GLuint shader_frag_average_x;
    GLuint program_average_x;
    GLuint shader_frag_average_y;
    GLuint program_average_y;

    GLuint u_texture_spin;
    GLuint u_q_colors;
    GLuint u_q;
    GLuint u_dx;
    GLuint u_ds_x;
    GLuint u_view_min_x;
    GLuint u_view_max_x;
    GLuint u_alpha_value_x;

    GLuint u_texture_x_averaged;
    GLuint u_dy;
    GLuint u_ds_y;
    GLuint u_view_min_y;
    GLuint u_view_max_y;
    GLuint u_alpha_value_y;
};

struct State
{
    int metropolis_steps_per_second;
    int N; // N_X*N_Y
    float zoom; // The zoom level
    float view_min_x, view_min_y, view_max_x, view_max_y;
    float brightness_of_non_primary_unit_cells;

    // Stores the physical settings of the state.
    PhysicalSettings ph_settings;
    // Stores newly input physical settings.
    PhysicalSettings ph_settings_input;
    // Both physical settings are synchronized by validate_state

    RenderState rstate;

    std::vector<uint8_t> spins; // the actual spins

    std::vector<int> other_Q; // 2d Array with size Qx(Q-1)
    int tp_offset_0, tp_offset_1; // Some constants for faster access in transition_precompute
    std::vector<float> transition_precompute; // precompute transition probabilities

    std::vector<float> colors;
    std::vector<float> colors_cache;

    std::uniform_real_distribution<double> dist_unit;// (0, 1); // random gen. [0,1)
    std::uniform_int_distribution<int> dist_lattice;// (0, 1); // random gen. for picking lattice site [0,width*height)
    std::uniform_int_distribution<int> dist_spin;// (0, 1); // random gen. fro picking spin [0,Q-1)
};

void GLAPIENTRY
MessageCallback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
    if (type == GL_DEBUG_TYPE_ERROR)
    {
        fprintf(stderr, "(%i) GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
                DEBUG_LINE_NUMBER,
                (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
                type, severity, message);
    }
    else
    {
        fprintf(stdout, "(%i) GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
                DEBUG_LINE_NUMBER,
                (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
                type, severity, message);
    }
}

float critical_temperature(int Q)
{
    // Taken from https://en.wikipedia.org/wiki/Potts_model#Phase_transitions
    return 1.0f / log(1.0f + sqrt( (float)(Q) ));
}

//Seriously why is it so hard?
std::string load_file_into_string(std::string filename)
{
    std::ifstream file(filename);
    std::string ret;
    file.seekg(0, std::ios::end);
    ret.reserve(file.tellg());
    file.seekg(0, std::ios::beg);
    ret.assign((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    file.close();
    return ret;
}

bool init_virtual_input()
{
    // All this should be read from a file in some way

    VirtualInputs &vi = G.virtual_inputs;
    //vi.up_down.configure(DeviceType::Keyboard, SDL_SCANCODE_W, SDL_SCANCODE_S);
    //vi.right_left.configure(DeviceType::Keyboard, SDL_SCANCODE_D, SDL_SCANCODE_A);

    vi.right_left.configure(DeviceType::Mouse, 1, 0);
    vi.up_down.configure(DeviceType::Mouse, 1, 1);
    vi.zoom.configure(DeviceType::Mouse, 2, 1);
    vi.drag.configure(DeviceType::Mouse, 0, 0);

    return true;
}

void validate_state(State* state)
{
    State &s = *state;
    PhysicalSettings &po = s.ph_settings;
    PhysicalSettings &pn = s.ph_settings_input;

    bool is_hamiltonian_changed = false;
    bool is_state_space_changed = false;

    pn.N_X = std::min(std::max(pn.N_X, G.POTTS_X_MIN), G.POTTS_X_MAX);
    pn.N_Y = std::min(std::max(pn.N_Y, G.POTTS_Y_MIN), G.POTTS_Y_MAX);
    s.N = pn.N_X * pn.N_Y;
    if (pn.N_X != po.N_X || pn.N_Y != po.N_Y)
    {
        // We do that here instead of every frame?
        s.dist_unit = std::uniform_real_distribution<double>(0, 1);

        s.dist_lattice = std::uniform_int_distribution<int>(0, s.N-1);
        s.spins.resize(s.N, 255); // Fill in max value, later we will correct that
        is_state_space_changed = true;
    }

    pn.Q = std::min(std::max(pn.Q, 2), G.POTTS_Q_MAX);
    if (pn.Q != po.Q)
    {
        is_hamiltonian_changed = true;
        pn.field.resize(pn.Q, 0.0);
        s.dist_spin = std::uniform_int_distribution<int>(0,pn.Q - 2);
        s.other_Q.resize(pn.Q * (pn.Q - 1));

        for (int q = 0; q < pn.Q; ++q)
            for (int r = 0; r < (pn.Q - 1); ++r)
            {
                s.other_Q[q * (pn.Q - 1) + r] = (r < q ? r : r + 1);
            }

        s.colors.resize(3*pn.Q, 0.0);
        for (int i = 0; i < 3*pn.Q; ++i)
            s.colors[i] = s.colors_cache[i];
        is_state_space_changed = true;
    }
    else
    {
        // check if fields have changed
        for (int q = 0; q < pn.Q; ++q)
            is_hamiltonian_changed |= (pn.field[q] != po.field[q]);
    }

    // Replace invalid spins with random value
    if (is_state_space_changed)
        for(int j = 0; j < s.N; ++j)
        {
            if (s.spins[j] >= pn.Q)
                s.spins[j] = s.dist_spin(G.rgen);
        }


    is_hamiltonian_changed |= (pn.temperature != po.temperature);
    is_hamiltonian_changed |= (pn.interaction != po.interaction);

    if(is_hamiltonian_changed)
    {
        s.transition_precompute.resize(pn.Q * pn.Q * 5 * 5);
        s.tp_offset_0 = pn.Q * 5 * 5;
        s.tp_offset_1 = 5 * 5;
        for(int old_spin = 0; old_spin  < pn.Q; ++old_spin)
        {
            for(int new_spin = 0; new_spin < pn.Q; ++new_spin)
            {
                for(int old_neighbors = 0; old_neighbors < 5; ++old_neighbors)
                {
                    for(int new_neighbors = 0; new_neighbors < 5; ++new_neighbors)
                    {
                        float delta_e =  (-pn.interaction * new_neighbors - pn.field[new_spin]) - (-pn.interaction * old_neighbors - pn.field[old_spin]);
                        float prob = std::min(1.0f, std::exp(-delta_e / pn.temperature));
                        s.transition_precompute[old_spin * s.tp_offset_0 + new_spin * s.tp_offset_1 + old_neighbors * 5 + new_neighbors] = prob;
                    }
                }
            }
        }
    }
    // Synchronize
    po = pn;
}

void render_state(State* state, bool cached)
{
    const State &s = *state;
    const PhysicalSettings &p = s.ph_settings;
    RenderState &rs = state->rstate;



    //////////////////////////
    // Doing the RG on the GPU

    LINE glActiveTexture(GL_TEXTURE0 + 0);
    // Define target texture for x averaging
    LINE glBindTexture(GL_TEXTURE_2D, rs.texture_x_averaged);
    // Reserve memory for the colorized and x-averaged texture. Note that x size is that of the screen
    LINE glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, G.wsize.x, p.N_Y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // Bind x-averaged framebuffer
    LINE glBindFramebuffer(GL_FRAMEBUFFER, rs.framebuffer_x_averaged);
    LINE glViewport(0, 0, G.wsize.x, p.N_Y);
    // Set the reserved texture as output of framebuffer
    LINE glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rs.texture_x_averaged, 0);

    // Upload spins
    LINE glBindTexture(GL_TEXTURE_2D, rs.texture_spin);
    LINE glTexImage2D(GL_TEXTURE_2D, 0, GL_R8I, p.N_X, p.N_Y, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &s.spins[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // GL_REPEAT
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    LINE glUseProgram(rs.program_average_x);
    // Use texture_spin as uniform
    LINE glUniform1i(rs.u_texture_spin, 0);
    // Set the other uniforms
    //std::cout << "Loop" << std::endl;
    glUniform3fv(rs.u_q_colors, 3*p.Q, &s.colors_cache[0]);
    /*
    for(int q = 0; q < p.Q;  ++q)
    {
        const float3 &rgb = s.colors_cache[q];
        glUniform3f(rs.u_q_colors + 16*q, rgb.x, rgb.y, rgb.z);
        //int adress = (int) (&s.colors_cache[q]);
        //std::cout << adress << std::endl;
        //glUniform3fv(rs.u_q_colors + 3 * q, 3, (const float *) (&s.colors_cache[q]));
    }
     */
    LINE glUniform1i(rs.u_q, p.Q);
    //LINE glUniform1f(rs.u_dx, (float)p.N_X / G.fwsize.x);
    LINE glUniform1f(rs.u_dx, (s.view_max_x-s.view_min_x) / G.wsize.x);
    LINE glUniform1f(rs.u_ds_x, 1.0f / p.N_X);
    LINE glUniform1f(rs.u_view_min_x, s.view_min_x);
    LINE glUniform1f(rs.u_view_max_x, s.view_max_x);
    glUniform1f(rs.u_alpha_value_x, s.brightness_of_non_primary_unit_cells);
    // Do the draw

    // LINE glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // LINE glViewport(0, 0, G.wsize.x, G.wsize.y);

    LINE glBindVertexArray(rs.vao_dummy);
    LINE glDrawArrays(GL_TRIANGLES, 0, 3);

    // Download texture
    //static char* pixels = nullptr;
    //if (!pixels)
    //    pixels = new char[p.N_Y * G.wsize.x * 3];
    //LINE glBindTexture(GL_TEXTURE_2D, rs.texture_x_averaged);
    //glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // Reset framebuffer to screen
    LINE glBindFramebuffer(GL_FRAMEBUFFER, 0);
    LINE glViewport(0, 0, G.wsize.x, G.wsize.y);
    // Use the recent result as texture
    LINE glBindTexture(GL_TEXTURE_2D, rs.texture_x_averaged);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // GL_REPEAT
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // GL_REPEAT
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    LINE glUseProgram(rs.program_average_y);
    LINE glUniform1i(rs.u_texture_x_averaged, 0);
    LINE glUniform1f(rs.u_dy, (s.view_max_y-s.view_min_y) / G.wsize.y);
    LINE glUniform1f(rs.u_ds_y, 1.0f / p.N_Y);
    LINE glUniform1f(rs.u_view_min_y, s.view_min_y);
    LINE glUniform1f(rs.u_view_max_y, s.view_max_y);
    glUniform1f(rs.u_alpha_value_y, s.brightness_of_non_primary_unit_cells);

    //LINE glBindVertexArray(rs.vao_dummy);
    LINE glDrawArrays(GL_TRIANGLES, 0, 3);

}

void render_gui(State* state)
{
    State &s = *state;
    PhysicalSettings &p = s.ph_settings_input;

    ImGui::Begin("System parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    static int n_x = p.N_X;
    static int n_y = p.N_Y;
    ImGui::SliderInt("Q", &p.Q, 2, G.POTTS_Q_MAX);
    ImGui::SliderInt("Size x", &n_x, G.POTTS_X_MIN, G.POTTS_X_MAX);
    bool slider_x_active = ImGui::IsItemActive();
    ImGui::SliderInt("Size y", &n_y, G.POTTS_Y_MIN, G.POTTS_Y_MAX);
    bool slider_y_active = ImGui::IsItemActive();

    if(!slider_x_active && !slider_y_active)
    {
        p.N_X = std::min(G.POTTS_X_MAX, std::max(G.POTTS_X_MIN, 4 * (n_x / 4)));
        p.N_Y = std::min(G.POTTS_Y_MAX, std::max(G.POTTS_Y_MIN, 4 * (n_y / 4)));
    }

    ImGui::SliderFloat("Brightness of non-primary unit cells", &s.brightness_of_non_primary_unit_cells, 0.0, 1.0);

    for(int q=0; q < p.Q; ++q)
    {
        std::string label = "Spin " + std::to_string(q) + " color";
        ImGui::ColorEdit3(label.c_str(), &s.colors_cache[3*q]);
    }

    if(ImGui::Button("Reset to transition point."))
        p.temperature = critical_temperature(p.Q);
    ImGui::SameLine();
    ImGui::SliderFloat("Temperature", &p.temperature, 0.0001, 8.0, "%.10f");


    if(ImGui::Button("Zero"))
        p.field[0] = 0.0;
    ImGui::SameLine();
    ImGui::SliderFloat("External field", &p.field[0], -2.0, 2.0);

    ImGui::SliderInt("Steps", &s.metropolis_steps_per_second, 1, 10000000);

    const VirtualInputs &vi = G.virtual_inputs;

    s.zoom *= exp(-0.05f*G.virtual_inputs.zoom.value);
    s.zoom = std::max(0.01f, std::min(s.zoom, 100.0f));

    float mx = (G.virtual_inputs.right_left.value + 1.0f) / 2.0f;
    float my = (G.virtual_inputs.up_down.value + 1.0f) / 2.0f;

    s.view_min_x += mx * (s.view_max_x - s.view_min_x - s.zoom);
    s.view_max_x = s.view_min_x + s.zoom;
    float zoom_y = s.zoom * G.fwsize.y * p.N_X / (G.fwsize.x * p.N_Y); // consider aspect ratio
    s.view_min_y += my * (s.view_max_y - s.view_min_y - zoom_y);
    s.view_max_y = s.view_min_y + zoom_y;

    if(G.virtual_inputs.drag.value && !ImGui::IsAnyItemActive())
    {
        s.view_min_x -= vi.right_left.delta * s.zoom / 2;
        s.view_max_x -= vi.right_left.delta * s.zoom / 2;
        s.view_min_y -= vi.up_down.delta * zoom_y / 2;
        s.view_max_y -= vi.up_down.delta * zoom_y / 2;
    }

    //ImGui::SliderInt("Spin updates per frame", &spin_updates_per_frame, 1, 1000000, "%d", ImGuiSliderFlags_Logarithmic);
    //ImGui::Checkbox("Zoom", &zoom_view);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();

    validate_state(state);
}

// Implemented from AppSDL


void init_app()
{
    std::cout << glGetString(GL_VENDOR) << std::endl;
    std::cout << glGetString(GL_RENDERER) << std::endl;
    std::cout << glGetString(GL_VERSION) << std::endl;
    //std::cout << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    //std::cout << glGetString(GL_EXTENSIONS) << std::endl;

    glEnable              ( GL_DEBUG_OUTPUT );
    glDebugMessageCallback( MessageCallback, 0 );

    SDL_SetWindowTitle(G.window, "Potts App");

    init_virtual_input();

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Init state
    G.state = new State{};
    State &s = *G.state;
    PhysicalSettings &p = G.state->ph_settings_input;

    p.N_X = G.POTTS_X_MIN;
    p.N_Y = G.POTTS_Y_MIN;
    p.Q = 2;
    p.temperature = critical_temperature(p.Q);
    p.interaction = 1.0; // Ferromagnetic convention
    s.metropolis_steps_per_second = 100;
    s.zoom = 1.0f;
    s.view_min_x = s.view_min_y = 0.0;
    s.view_max_x = s.view_max_y = 1.0;
    s.brightness_of_non_primary_unit_cells = 0.5f;

    // Matplotlib default colors
    s.colors_cache.resize(3*G.POTTS_Q_MAX, 0.0);
    s.colors_cache = {
        0.12, 0.47, 0.71,
        1.00, 0.50, 0.05,
        0.17, 0.63, 0.17,
        0.84, 0.15, 0.16,
        0.58, 0.40, 0.74,
        0.55, 0.34, 0.29,
        0.89, 0.47, 0.76,
        0.50, 0.50, 0.50,
        0.74, 0.74, 0.13,
        0.09, 0.75, 0.78};

    validate_state(&s);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Init render state

    RenderState &rs = s.rstate;


    glGenVertexArrays(1, &rs.vao_dummy);
    glGenTextures(1, &rs.texture_spin);
    glGenTextures(1, &rs.texture_x_averaged);
    glGenFramebuffers(1, &rs.framebuffer_x_averaged);

    //////////
    // Shaders
    rs.shader_vert = glCreateShader(GL_VERTEX_SHADER);
    rs.shader_frag_average_x = glCreateShader(GL_FRAGMENT_SHADER);
    rs.shader_frag_average_y = glCreateShader(GL_FRAGMENT_SHADER);
    rs.program_average_x = glCreateProgram();
    rs.program_average_y = glCreateProgram();

    const int info_max = 16384;
    char* info = new char[info_max];

    std::string shader_source_vert = load_file_into_string("data/quad.vert");
    const char* src = shader_source_vert.c_str();
    glShaderSource(rs.shader_vert, 1, &src, nullptr);
    glCompileShader(rs.shader_vert);
    glGetShaderInfoLog(rs.shader_vert, info_max, NULL, info);
    printf("quad.vert: %s\n", info);

    std::string shader_source_frag = load_file_into_string("data/average_spins_x.frag");
    src = shader_source_frag.c_str();
    glShaderSource(rs.shader_frag_average_x, 1, &src, nullptr);
    glCompileShader(rs.shader_frag_average_x);
    glGetShaderInfoLog(rs.shader_frag_average_x, info_max, NULL, info);
    printf("average_spins_x.frag: %s\n", info);

    //shader_source_frag = load_file_into_string("data/test.frag");
    shader_source_frag = load_file_into_string("data/average_color_y.frag");
    src = shader_source_frag.c_str();
    glShaderSource(rs.shader_frag_average_y, 1, &src, nullptr);
    glCompileShader(rs.shader_frag_average_y);
    glGetShaderInfoLog(rs.shader_frag_average_y, info_max, NULL, info);
    printf("average_spins_y.frag: %s\n", info);

    glAttachShader(rs.program_average_x, rs.shader_vert);
    glAttachShader(rs.program_average_x, rs.shader_frag_average_x);
    glBindAttribLocation(rs.program_average_x, 0, "texcoords");
    glBindAttribLocation(rs.program_average_x, 1, "color");
    glLinkProgram(rs.program_average_x);
    glGetProgramInfoLog(rs.program_average_x, info_max, NULL, info);
    printf("program_average_x: %s\n", info);

    // Get uniform locations
    rs.u_alpha_value_x = glGetUniformLocation(rs.program_average_x, "alpha_value");
    rs.u_texture_spin = glGetUniformLocation(rs.program_average_x, "texture_spin");
    rs.u_q_colors = glGetUniformLocation(rs.program_average_x, "q_colors");
    rs.u_q = glGetUniformLocation(rs.program_average_x, "q");
    rs.u_dx = glGetUniformLocation(rs.program_average_x, "dx");
    rs.u_ds_x = glGetUniformLocation(rs.program_average_x, "ds");
    rs.u_view_min_x = glGetUniformLocation(rs.program_average_x, "view_min");
    rs.u_view_max_x = glGetUniformLocation(rs.program_average_x, "view_max");

    glAttachShader(rs.program_average_y, rs.shader_vert);
    glAttachShader(rs.program_average_y, rs.shader_frag_average_y);
    glBindAttribLocation(rs.program_average_y, 0, "texcoords");
    glBindAttribLocation(rs.program_average_y, 1, "color");
    glLinkProgram(rs.program_average_y);
    glGetProgramInfoLog(rs.program_average_y, info_max, NULL, info);
    printf("program_average_y: %s\n", info);

    // Get uniform locations
    rs.u_alpha_value_y = glGetUniformLocation(rs.program_average_x, "alpha_value");
    rs.u_texture_x_averaged = glGetUniformLocation(rs.program_average_y, "texture_x_averaged");
    rs.u_dy = glGetUniformLocation(rs.program_average_y, "dy");
    rs.u_ds_y = glGetUniformLocation(rs.program_average_y, "ds");
    rs.u_view_min_y = glGetUniformLocation(rs.program_average_y, "view_min");
    rs.u_view_max_y = glGetUniformLocation(rs.program_average_y, "view_max");

    delete[] info;
    fflush(stdout);
}

void destroy_app()
{
    delete G.state;
}

void integrate_state(State* state, float dt)
{
    // Apply monte carlo method here

    State &s = *state;
    const PhysicalSettings &p = s.ph_settings;

    int numsteps = (int)(dt * s.metropolis_steps_per_second);
    for(int it_step = 0; it_step < numsteps; ++it_step)
    {
        // Draw random site which me might want to flip
        int j = s.dist_lattice(G.rgen);

        // Calculate its coordinates
        int y = j / p.N_X;
        int x = j % p.N_X;

        int j_nn[4]; // Nearest neighbors of j (Torus)
        j_nn[0] = j - 1 + p.N_X * (x == 0); // left
        j_nn[1] = j - p.N_X + p.N_X * p.N_Y * (y == 0); // up
        j_nn[2] = j + 1 - p.N_X * (x == p.N_X - 1); // right
        j_nn[3] = j + p.N_X - p.N_X * p.N_Y * (y == p.N_Y - 1); // down

        // Current state
        int old_spin = s.spins[j];
        // Random candidate as other state for spin flip
        int new_spin = s.other_Q[old_spin * (p.Q - 1) + s.dist_spin(G.rgen)];

        int old_neighbors = 0;
        int new_neighbors = 0;
        for(int k = 0; k < 4; ++k)
        {
            old_neighbors += (old_spin == s.spins[j_nn[k]]);
            new_neighbors += (new_spin == s.spins[j_nn[k]]);
        }
        // old energy sum may have the values: 0, 1, 2, 3, 4
        // new energy sum may have the values: 0, 1, 2, 3, 4
        // in total we have a matrix with dimension QxQx5x5. Elements in (q,q,...,...) are not defined

        float prob = s.transition_precompute[old_spin * s.tp_offset_0 + new_spin * s.tp_offset_1 + old_neighbors * 5 + new_neighbors];
        float r = s.dist_unit(G.rgen);
        if (r <= prob)
        {
            s.spins[j] = new_spin;
            // Perform measurements
        }
    }
}
