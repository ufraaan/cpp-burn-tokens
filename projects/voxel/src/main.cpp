#include <SDL2/SDL.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include "terrain.hpp"

static constexpr int W_WIDTH = 1200;
static constexpr int W_HEIGHT = 800;

struct Camera {
    float theta = 0.5f;     // vertical angle (radians)
    float phi = 0.0f;       // horizontal angle
    float radius = 8.0f;
    float targetTheta = 0.5f;
    float targetPhi = 0.0f;
    float targetRadius = 8.0f;
    bool autoRotate = true;

    void update(float dt) {
        if (autoRotate) phi += dt * 0.15f;
        theta += (targetTheta - theta) * 0.1f;
        phi += (targetPhi - phi) * 0.1f;
        radius += (targetRadius - radius) * 0.1f;
    }

    void look() const {
        float x = radius * std::sin(theta) * std::sin(phi);
        float y = radius * std::cos(theta);
        float z = radius * std::sin(theta) * std::cos(phi);
        gluLookAt(x, y, z, 0, 0, 0, 0, 1, 0);
    }
};

static void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat lightPos[] = {10.0f, 15.0f, 10.0f, 1.0f};
    GLfloat ambient[] = {0.25f, 0.25f, 0.3f, 1.0f};
    GLfloat diffuse[] = {0.9f, 0.9f, 1.0f, 1.0f};
    GLfloat specular[] = {0.3f, 0.3f, 0.3f, 1.0f};

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);

    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
}

static void setupFog() {
    glEnable(GL_FOG);
    GLfloat fogColor[] = {0.05f, 0.05f, 0.1f, 1.0f};
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_MODE, GL_EXP2);
    glFogf(GL_FOG_DENSITY, 0.012f);
    glHint(GL_FOG_HINT, GL_NICEST);
}

static void renderWaterPlane() {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.02f, 0.05f, 0.15f, 0.6f);
    float s = 6.0f;
    glBegin(GL_QUADS);
    glVertex3f(-s, -0.3f, -s);
    glVertex3f(s, -0.3f, -s);
    glVertex3f(s, -0.3f, s);
    glVertex3f(-s, -0.3f, s);
    glEnd();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

int main() {
    std::srand(std::time(nullptr));

    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window* win = SDL_CreateWindow("Voxel — 3D Terrain Explorer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W_WIDTH, W_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1); // vsync

    Terrain terrain(160, 0.06f, 1.0f);
    terrain.generate(std::time(nullptr));

    Camera cam;
    bool running = true;
    bool wireframe = false;
    bool showWater = true;
    int lastMouseX = -1, lastMouseY = -1;
    bool mouseDown = false;
    Uint32 lastFrame = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_r:
                        terrain.generate(std::time(nullptr) + std::rand());
                        break;
                    case SDLK_w:
                        wireframe = !wireframe;
                        break;
                    case SDLK_SPACE:
                        cam.autoRotate = !cam.autoRotate;
                        break;
                    case SDLK_x:
                        showWater = !showWater;
                        break;
                }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                mouseDown = true;
                lastMouseX = e.button.x;
                lastMouseY = e.button.y;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT)
                mouseDown = false;

            if (e.type == SDL_MOUSEMOTION && mouseDown) {
                float dx = (float)(e.motion.x - lastMouseX) * 0.005f;
                float dy = (float)(e.motion.y - lastMouseY) * 0.005f;
                cam.targetPhi -= dx;
                cam.targetTheta += dy;
                cam.targetTheta = (cam.targetTheta < 0.1f) ? 0.1f :
                                  (cam.targetTheta > 2.8f) ? 2.8f : cam.targetTheta;
                lastMouseX = e.motion.x;
                lastMouseY = e.motion.y;
            }

            if (e.type == SDL_MOUSEWHEEL) {
                cam.targetRadius *= (e.wheel.y > 0) ? 0.9f : 1.1f;
                if (cam.targetRadius < 2.0f) cam.targetRadius = 2.0f;
                if (cam.targetRadius > 30.0f) cam.targetRadius = 30.0f;
            }

            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
                int nw = e.window.data1, nh = e.window.data2;
                if (nh < 1) nh = 1;
                glViewport(0, 0, nw, nh);
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastFrame) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastFrame = now;

        cam.update(dt);

        // Resize-aware viewport
        int vw, vh;
        SDL_GetWindowSize(win, &vw, &vh);
        if (vh < 1) vh = 1;
        glViewport(0, 0, vw, vh);

        // Clear
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // Projection
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)vw / vh, 0.1, 50.0);

        // Model-view
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        cam.look();

        setupLighting();
        setupFog();

        // Render terrain
        if (wireframe) {
            terrain.renderWireframe();
        } else {
            terrain.render();
        }

        if (showWater) renderWaterPlane();

        SDL_GL_SwapWindow(win);

        // Frame rate
        Uint32 frameTime = SDL_GetTicks() - now;
        if (frameTime < 16) SDL_Delay(16 - frameTime);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
