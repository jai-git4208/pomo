#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define wd (25 * 60)
#define bd (5 * 60)
#define wh 400
#define ww 400
#define FONT_PATH "/System/Library/Fonts/Supplemental/Arial.ttf"

typedef enum { w, b } State;

typedef struct {
    State state;
    int sec_remain;
    bool paused;
    uint32_t last_tick;
} Timer;

void drawRing(float x, float y, float outerR, float innerR,
              float start_angle, float end_angle,
              float r, float g, float b) {

    glColor3f(r, g, b);
    glBegin(GL_TRIANGLE_STRIP);

    int segments = 100;
    for (int i = 0; i <= segments; i++) {
        float angle = start_angle + (end_angle - start_angle) * (i / (float)segments);
        float rad = (angle - 90.0f) * M_PI / 180.0f;

        glVertex2f(x + cos(rad) * outerR, y + sin(rad) * outerR);
        glVertex2f(x + cos(rad) * innerR, y + sin(rad) * innerR);
    }

    glEnd();
}

void rendertexty(TTF_Font *font, const char *text, SDL_Color color,
                float x, float y, bool center) {

    SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) return;

    SDL_Surface *opt = SDL_ConvertSurfaceFormat(
        surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(surface);
    if (!opt) return;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA,
        opt->w, opt->h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, opt->pixels
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(1, 1, 1, 1);  

    float dx = x;
    float dy = y;
    if (center) {
        dx -= opt->w / 2.0f;
        dy -= opt->h / 2.0f;
    }

    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(dx, dy);

        glTexCoord2f(1, 0);
        glVertex2f(dx + opt->w, dy);

        glTexCoord2f(1, 1);
        glVertex2f(dx + opt->w, dy + opt->h);

        glTexCoord2f(0, 1);
        glVertex2f(dx, dy + opt->h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDeleteTextures(1, &tex);
    SDL_FreeSurface(opt);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window *window = SDL_CreateWindow(
        "pomo",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ww, wh,
        SDL_WINDOW_OPENGL
    );

    SDL_GLContext ctx = SDL_GL_CreateContext(window);

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, ww, wh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    TTF_Font *font_big = TTF_OpenFont(FONT_PATH, 80);
    TTF_Font *font_small = TTF_OpenFont(FONT_PATH, 20);

    Timer timer = { w, wd, false, SDL_GetTicks() };
    bool running = true;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE)
                timer.paused = !timer.paused;
        }

        uint32_t now = SDL_GetTicks();
        if (!timer.paused && now - timer.last_tick >= 1000) {
            timer.sec_remain--;
            timer.last_tick = now;

            if (timer.sec_remain < 0) {
                timer.state = (timer.state == w) ? b : w;
                timer.sec_remain = (timer.state == w) ? wd : bd;
            }
        }

        glClearColor(0.12f, 0.14f, 0.22f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glLoadIdentity();

        drawRing(ww / 2, wh / 2, 162, 148, 0, 360, 0.2f, 0.2f, 0.3f);

        int total = (timer.state == w) ? wd : bd;
        float progress = (float)timer.sec_remain / total;
        drawRing(ww / 2, wh / 2, 162, 148, 0, 360 * progress, 1, 0.4f, 0.4f);

        char buf[16];
        sprintf(buf, "%02d:%02d", timer.sec_remain / 60, timer.sec_remain % 60);

        SDL_Color white = {255, 255, 255, 255};
        SDL_Color gray = {180, 180, 180, 255};

        rendertexty(font_big, buf, white, ww / 2, wh / 2 - 10, true);
        rendertexty(font_small,
                     timer.paused ? "PAUSED" : (timer.state == w ? "WORK" : "BREAK"),
                     gray, ww / 2, wh / 2 + 60, true);

        SDL_GL_SwapWindow(window);
        SDL_Delay(16);
    }

    TTF_CloseFont(font_big);
    TTF_CloseFont(font_small);
    TTF_Quit();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
