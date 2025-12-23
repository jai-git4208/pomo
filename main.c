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
  double sec_remain;
  bool paused;
  uint32_t last_frame_time;
} Timer;

typedef struct {
  GLuint texture;
  int w, h;
  char text[32];
  SDL_Color color;
} CachedText;

void draw_filled_circle(float x, float y, float rad, float r, float g, float b,
                        float a) {
  glBegin(GL_TRIANGLE_FAN);
  glColor4f(r, g, b, a);
  glVertex2f(x, y);
  int segments = 40;
  for (int i = 0; i <= segments; i++) {
    float angle = 2.0f * M_PI * i / segments;
    glVertex2f(x + cos(angle) * rad, y + sin(angle) * rad);
  }
  glEnd();
}

void draw_ring_segment(float x, float y, float outerR, float innerR,
                       float start_angle, float end_angle, float r, float g,
                       float b, float a) {
  glBegin(GL_TRIANGLE_STRIP);
  glColor4f(r, g, b, a);
  int segments = (int)(fabs(end_angle - start_angle) / 2.0f) + 10;
  if (segments < 10)
    segments = 10;

  for (int i = 0; i <= segments; i++) {
    float angle =
        start_angle + (end_angle - start_angle) * (i / (float)segments);
    float rad = (angle - 90.0f) * M_PI / 180.0f;
    glVertex2f(x + cos(rad) * outerR, y + sin(rad) * outerR);
    glVertex2f(x + cos(rad) * innerR, y + sin(rad) * innerR);
  }
  glEnd();

  // round round caps :3
  float mid_r = (outerR + innerR) / 2.0f;
  float cap_r = (outerR - innerR) / 2.0f;

  float start_rad = (start_angle - 90.0f) * M_PI / 180.0f;
  draw_filled_circle(x + cos(start_rad) * mid_r, y + sin(start_rad) * mid_r,
                     cap_r, r, g, b, a);
  float end_rad = (end_angle - 90.0f) * M_PI / 180.0f;
  draw_filled_circle(x + cos(end_rad) * mid_r, y + sin(end_rad) * mid_r, cap_r,
                     r, g, b, a);
}

void update_cached_text(TTF_Font *font, CachedText *cache, const char *text,
                        SDL_Color color) {
  if (cache->texture != 0 && strcmp(cache->text, text) == 0 &&
      cache->color.r == color.r && cache->color.g == color.g &&
      cache->color.b == color.b) {
    return;
  }
  if (cache->texture != 0) {
    glDeleteTextures(1, &cache->texture);
    cache->texture = 0;
  }
  SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
  if (!surface)
    return;

  SDL_Surface *opt =
      SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(surface);
  if (!opt)
    return;

  glGenTextures(1, &cache->texture);
  glBindTexture(GL_TEXTURE_2D, cache->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, opt->w, opt->h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, opt->pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  cache->w = opt->w;
  cache->h = opt->h;
  strncpy(cache->text, text, sizeof(cache->text) - 1);
  cache->color = color;
  SDL_FreeSurface(opt);
}

void draw_cached_text(CachedText *cache, float x, float y, bool center,
                      float alpha) {
  if (cache->texture == 0)
    return;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, cache->texture);
  glColor4f(1.0f, 1.0f, 1.0f, alpha);

  float dx = x;
  float dy = y;
  if (center) {
    dx -= cache->w / 2.0f;
    dy -= cache->h / 2.0f;
  }

  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(dx, dy);
  glTexCoord2f(1, 0);
  glVertex2f(dx + cache->w, dy);
  glTexCoord2f(1, 1);
  glVertex2f(dx + cache->w, dy + cache->h);
  glTexCoord2f(0, 1);
  glVertex2f(dx, dy + cache->h);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}
void drawRing(float x, float y, float outerR, float innerR, float start_angle,
              float end_angle, float r, float g, float b) {

  glColor3f(r, g, b);
  glBegin(GL_TRIANGLE_STRIP);

  int segments = 100;
  for (int i = 0; i <= segments; i++) {
    float angle =
        start_angle + (end_angle - start_angle) * (i / (float)segments);
    float rad = (angle - 90.0f) * M_PI / 180.0f;

    glVertex2f(x + cos(rad) * outerR, y + sin(rad) * outerR);
    glVertex2f(x + cos(rad) * innerR, y + sin(rad) * innerR);
  }

  glEnd();
}

void rendertexty(TTF_Font *font, const char *text, SDL_Color color, float x,
                 float y, bool center) {

  SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
  if (!surface)
    return;

  SDL_Surface *opt =
      SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(surface);
  if (!opt)
    return;

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, opt->w, opt->h, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, opt->pixels);

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

SDL_HitTestResult drag_hit_test(SDL_Window *window, const SDL_Point *area,
                                void *data) {
  return SDL_HITTEST_DRAGGABLE;
}

int main(int argc, char *argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return 1;
  TTF_Init();

  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  // wimndow

  SDL_Window *window = SDL_CreateWindow("pomopomo", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, ww, wh,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

  SDL_SetWindowHitTest(window, drag_hit_test, NULL);

  SDL_GLContext context = SDL_GL_CreateContext(window);
  SDL_GL_SetSwapInterval(1); // VSYNCCCCCCC!!!

  TTF_Font *font_large = TTF_OpenFont(FONT_PATH, 90);
  TTF_Font *font_small = TTF_OpenFont(FONT_PATH, 16);

  Timer timer = {w, (double)wd, false, SDL_GetTicks()};
  CachedText time_cache = {0}, label_cache = {0};
  bool running = true;
  SDL_Event e;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_MULTISAMPLE);

  while (running) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = false;
      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE)
        timer.paused = !timer.paused;
    }
    uint32_t now = SDL_GetTicks();
    double dt = (now - timer.last_frame_time) / 1000.0;
    timer.last_frame_time = now;

    if (!timer.paused) {
      timer.sec_remain -= dt;
      if (timer.sec_remain <= 0) {
        if (timer.state == w) {
          timer.state = b;
          timer.sec_remain = bd;
        } else {
          timer.state = w;
          timer.sec_remain = wd;
        }
      }
    }

    glClearColor(0.10f, 0.12f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, ww, wh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // back ringy
    draw_ring_segment(ww / 2.0f, wh / 2.0f, 160, 150, 0, 360, 0.15f, 0.17f,
                      0.25f, 1.0f);

    // progress
    double total = (timer.state == w) ? wd : bd;
    float progress = (float)(timer.sec_remain / total);
    if (progress < 0)
      progress = 0;

    float ring_r = (timer.state == w) ? 1.0f : 0.4f;
    float ring_g = (timer.state == w) ? 0.4f : 0.8f;
    float ring_b = (timer.state == w) ? 0.4f : 1.0f;
    draw_ring_segment(ww / 2.0f, wh / 2.0f, 160, 150, 0, 360.0f * progress,
                      ring_r, ring_g, ring_b, 1.0f);

    // time txt
    char spiderman[16];
    int display_secs = (int)ceil(timer.sec_remain);
    sprintf(spiderman, "%02d:%02d", display_secs / 60, display_secs % 60);
    SDL_Color white = {255, 255, 255, 255};
    update_cached_text(font_large, &time_cache, spiderman, white);

    float alpha = 1.0f;
    if (timer.paused) {
      alpha = 0.7f + 0.3f * sin(now * 0.005f);
    }
    draw_cached_text(&time_cache, ww / 2.0f, wh / 2.0f - 10, true, alpha);

    // label
    const char *label =
        timer.paused
            ? "PAUSED WORK BROOOOOO"
            : (timer.state == w ? "GOOD BOY WORKING :3" : "BREAKING! ENJOYYYY");
    SDL_Color gray = {160, 170, 200, 255};
    update_cached_text(font_small, &label_cache, label, gray);
    draw_cached_text(&label_cache, ww / 2.0f, wh / 2.0f + 70, true, 1.0f);
    SDL_GL_SwapWindow(window);
  }
  if (time_cache.texture)
    glDeleteTextures(1, &time_cache.texture);
  if (label_cache.texture)
    glDeleteTextures(1, &label_cache.texture);
  TTF_CloseFont(font_large);
  TTF_CloseFont(font_small);
  TTF_Quit();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}