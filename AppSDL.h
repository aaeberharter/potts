//
// Created by alex on 09/04/2022.
//

#ifndef CPP_SDL2_APPSDL_H
#define CPP_SDL2_APPSDL_H

#include <imgui.h>

// What does this define do?
#define GL_GLEXT_PROTOTYPES 1
//#include <SDL_opengles2.h> // WebGL 1, OpenGL ES 2.0
//#include <gl/GL.h> // DOES NOT WORK. Desktop OpenGL 4.3, compatible with WebGL 2, OpenGL ES 3.0?
#include <GL/glew.h> // Desktop OpenGL 4.3, compatible with WebGL 2, OpenGL ES 3.0?

#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

// Implement these to make your app.

// Init your app. Remember to initialize states!
void init_app();
// Free resources. Remember to free states!
void destroy_app();
// Integrate the state, please make sure it only takes a time of dt
void integrate_state(State *s, float dt);
// Render the state using slow method if cached=false, otherwise use a cached result
void render_state(State *s, bool cached);
// Render GUI
void render_gui(State *s);

#endif //CPP_SDL2_APPSDL_H
