#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
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
#define wh 200
#define ww 200
#define FONT_PATH "/System/Library/Fonts/Supplemental/Arial.ttf"
#define CONFIG_PATH "pomo.cfg"

typedef enum { w, b } State;

typedef struct {
  int work_min;
  int break_min;
  bool sound_on;
  int x, y;
} Config;

typedef struct {
  State state;
  double sec_remain;
  bool paused;
  uint32_t last_frame_time;
  double pause_duration;
  int base_x, base_y;
  bool is_shaking;
  Mix_Music *music;
  double elapsed_work;
  double elapsed_break;
  Config config;
  SDL_Window *settings_win;
  SDL_Renderer *settings_ren;
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
  if (!font)
    return;
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

void save_config(const Config *cfg) {
  FILE *f = fopen(CONFIG_PATH, "w");
  if (!f)
    return;
  fprintf(f, "work_time=%d\n", cfg->work_min);
  fprintf(f, "break_time=%d\n", cfg->break_min);
  fprintf(f, "sound=%d\n", cfg->sound_on ? 1 : 0);
  fprintf(f, "x=%d\n", cfg->x);
  fprintf(f, "y=%d\n", cfg->y);
  fclose(f);
}

void load_config(Config *cfg) {
  cfg->work_min = 25;
  cfg->break_min = 5;
  cfg->sound_on = true;
  cfg->x = SDL_WINDOWPOS_CENTERED;
  cfg->y = SDL_WINDOWPOS_CENTERED;

  FILE *f = fopen(CONFIG_PATH, "r");
  if (!f) {
    save_config(cfg);
    return;
  }
  char line[128];
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "work_time=%d", &cfg->work_min) == 1)
      continue;
    if (sscanf(line, "break_time=%d", &cfg->break_min) == 1)
      continue;
    if (sscanf(line, "sound=%d", (int *)&cfg->sound_on) == 1)
      continue;
    if (sscanf(line, "x=%d", &cfg->x) == 1)
      continue;
    if (sscanf(line, "y=%d", &cfg->y) == 1)
      continue;
  }
  fclose(f);
}

void reset_timer(Timer *timer, SDL_Window *window) {
  timer->state = w;
  timer->sec_remain = timer->config.work_min * 60.0;
  timer->paused = false;
  timer->elapsed_work = 0;
  timer->elapsed_break = 0;
  timer->pause_duration = 0;
  if (timer->is_shaking) {
    SDL_SetWindowPosition(window, timer->base_x, timer->base_y);
    timer->is_shaking = false;
  }
  if (timer->music && timer->config.sound_on) {
    Mix_SetMusicPosition(0);
    Mix_ResumeMusic();
  }
}

void open_settings_window(Timer *timer, TTF_Font *font) {
  if (timer->settings_win)
    return;

  timer->settings_win =
      SDL_CreateWindow("Settings", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 300, 150, SDL_WINDOW_SHOWN);
  timer->settings_ren =
      SDL_CreateRenderer(timer->settings_win, -1, SDL_RENDERER_ACCELERATED);
}

void render_settings(Timer *timer, TTF_Font *font) {
  if (!timer->settings_win)
    return;

  SDL_SetRenderDrawColor(timer->settings_ren, 30, 33, 40, 255);
  SDL_RenderClear(timer->settings_ren);

  SDL_Color white = {255, 255, 255, 255};
  char buf[64];

  // Work
  sprintf(buf, "Work: %d min (Use Up/Down)", timer->config.work_min);
  SDL_Surface *s = TTF_RenderText_Blended(font, buf, white);
  SDL_Texture *t = SDL_CreateTextureFromSurface(timer->settings_ren, s);
  SDL_Rect r = {20, 20, s->w, s->h};
  SDL_RenderCopy(timer->settings_ren, t, NULL, &r);
  SDL_FreeSurface(s);
  SDL_DestroyTexture(t);

  // Break
  sprintf(buf, "Break: %d min (Use Left/Right)", timer->config.break_min);
  s = TTF_RenderText_Blended(font, buf, white);
  t = SDL_CreateTextureFromSurface(timer->settings_ren, s);
  r = (SDL_Rect){20, 60, s->w, s->h};
  SDL_RenderCopy(timer->settings_ren, t, NULL, &r);
  SDL_FreeSurface(s);
  SDL_DestroyTexture(t);

  // Sound
  sprintf(buf, "Sound: %s (Use Enter)", timer->config.sound_on ? "ON" : "OFF");
  s = TTF_RenderText_Blended(font, buf, white);
  t = SDL_CreateTextureFromSurface(timer->settings_ren, s);
  r = (SDL_Rect){20, 100, s->w, s->h};
  SDL_RenderCopy(timer->settings_ren, t, NULL, &r);
  SDL_FreeSurface(s);
  SDL_DestroyTexture(t);

  SDL_RenderPresent(timer->settings_ren);
}

void snap_to_corner(SDL_Window *window) {
  int displayIndex = SDL_GetWindowDisplayIndex(window);
  if (displayIndex < 0)
    return;

  SDL_Rect usableBounds;
  if (SDL_GetDisplayUsableBounds(displayIndex, &usableBounds) != 0)
    return;

  int wx, wy, ww_local, wh_local;
  SDL_GetWindowPosition(window, &wx, &wy);
  SDL_GetWindowSize(window, &ww_local, &wh_local);

  // 4 corners of the usable bounds
  SDL_Point corners[] = {
      {usableBounds.x, usableBounds.y},                             // Top-Left
      {usableBounds.x + usableBounds.w - ww_local, usableBounds.y}, // Top-Right
      {usableBounds.x,
       usableBounds.y + usableBounds.h - wh_local}, // Bottom-Left
      {usableBounds.x + usableBounds.w - ww_local,
       usableBounds.y + usableBounds.h - wh_local} // Bottom-Right
  };

  SDL_Point nearestCorner = corners[0];
  long min_dist_sq = 2000000000; // Large enough

  for (int i = 0; i < 4; i++) {
    long dx = (long)wx - corners[i].x;
    long dy = (long)wy - corners[i].y;
    long dist_sq = dx * dx + dy * dy;
    if (dist_sq < min_dist_sq) {
      min_dist_sq = dist_sq;
      nearestCorner = corners[i];
    }
  }

  // Set window position. Note: SDL2 doesn't have built-in smooth animation
  // like Cocoa's animate:YES, so it will snap instantly. To animate,
  // one would need a custom interpolation loop.
  SDL_SetWindowPosition(window, nearestCorner.x, nearestCorner.y);

  // Save new position
  void *data = SDL_GetWindowData(window, "timer");
  if (data) {
    Timer *timer = (Timer *)data;
    timer->config.x = nearestCorner.x;
    timer->config.y = nearestCorner.y;
    save_config(&timer->config);
  }
}

int main(int argc, char *argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    return 1;
  TTF_Init();

  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  TTF_Font *font_small = TTF_OpenFont(FONT_PATH, 10);

  Timer timer = {w};
  timer.last_frame_time = SDL_GetTicks();
  load_config(&timer.config);
  timer.sec_remain = timer.config.work_min * 60.0;

  // wimndow
  SDL_Window *window = SDL_CreateWindow(
      "pomopomo", timer.config.x, timer.config.y, ww, wh,
      SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);

  if (!window) {
    fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
    return 1;
  }

  // If we used centered, set the real position now
  int actualX, actualY;
  SDL_GetWindowPosition(window, &actualX, &actualY);
  timer.config.x = actualX;
  timer.config.y = actualY;
  save_config(&timer.config);

  SDL_SetWindowHitTest(window, drag_hit_test, NULL);
  SDL_SetWindowData(window, "timer", &timer);
  CachedText time_cache = {0}, label_cache = {0};
  bool running = true;
  SDL_Event e;

  SDL_GLContext context = SDL_GL_CreateContext(window);
  if (!context) {
    fprintf(stderr, "GL Context creation failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_GL_MakeCurrent(window, context);
  SDL_GL_SetSwapInterval(1); // VSYNCCCCCCC!!!

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_MULTISAMPLE);

  TTF_Font *font_large = TTF_OpenFont(FONT_PATH, 40);
  if (!font_large) {
    fprintf(stderr, "Failed to load large font: %s\n", TTF_GetError());
  }
  if (!font_small) {
    fprintf(stderr, "Failed to load small font: %s\n", TTF_GetError());
  }

  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    fprintf(stderr, "SDL_mixer could not initialize! SDL_mixer Error: %s\n",
            Mix_GetError());
  }
  timer.music = Mix_LoadMUS("res/timer.mp3");
  if (timer.music && timer.config.sound_on) {
    Mix_PlayMusic(timer.music, -1);
  } else if (!timer.music) {
    fprintf(stderr, "Failed to load music: %s\n", Mix_GetError());
  }

  while (running) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = false;

      if (e.type == SDL_WINDOWEVENT &&
          e.window.event == SDL_WINDOWEVENT_CLOSE) {
        if (timer.settings_win &&
            e.window.windowID == SDL_GetWindowID(timer.settings_win)) {
          SDL_DestroyRenderer(timer.settings_ren);
          SDL_DestroyWindow(timer.settings_win);
          timer.settings_win = NULL;
          timer.settings_ren = NULL;
          save_config(&timer.config);
        } else {
          running = false;
        }
      }

      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_SPACE) {
          timer.paused = !timer.paused;
          if (timer.paused)
            Mix_PauseMusic();
          else
            Mix_ResumeMusic();
        } else if (e.key.keysym.sym == SDLK_r) {
          reset_timer(&timer, window);
        } else if (e.key.keysym.sym == SDLK_s) {
          if (!timer.settings_win)
            open_settings_window(&timer, font_small);
          else {
            SDL_DestroyRenderer(timer.settings_ren);
            SDL_DestroyWindow(timer.settings_win);
            timer.settings_win = NULL;
            timer.settings_ren = NULL;
            save_config(&timer.config);
          }
        }

        if (timer.settings_win) {
          if (e.key.keysym.sym == SDLK_UP)
            timer.config.work_min++;
          if (e.key.keysym.sym == SDLK_DOWN && timer.config.work_min > 1)
            timer.config.work_min--;
          if (e.key.keysym.sym == SDLK_RIGHT)
            timer.config.break_min++;
          if (e.key.keysym.sym == SDLK_LEFT && timer.config.break_min > 1)
            timer.config.break_min--;
          if (e.key.keysym.sym == SDLK_RETURN ||
              e.key.keysym.sym == SDLK_KP_ENTER) {
            timer.config.sound_on = !timer.config.sound_on;
            if (timer.config.sound_on)
              Mix_ResumeMusic();
            else
              Mix_HaltMusic();
          }

          // Auto update timer if changed
          timer.sec_remain = timer.config.work_min * 60.0;
          timer.elapsed_work = 0;
          timer.elapsed_break = 0;
          timer.state = w;
        }
      }
      if (e.type == SDL_WINDOWEVENT &&
          e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        snap_to_corner(window);
      }
      if (e.type == SDL_WINDOWEVENT &&
          e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        Config old = timer.config;
        load_config(&timer.config);
        if (old.work_min != timer.config.work_min ||
            old.break_min != timer.config.break_min) {
          reset_timer(&timer, window);
        }
        if (old.sound_on != timer.config.sound_on) {
          if (timer.config.sound_on) {
            Mix_PlayMusic(timer.music, -1);
          } else {
            Mix_HaltMusic();
          }
        }
      }
      if (e.type == SDL_MOUSEBUTTONUP) {
        int wx, wy;
        SDL_GetWindowPosition(window, &wx, &wy);
        timer.config.x = wx;
        timer.config.y = wy;
        save_config(&timer.config);
        snap_to_corner(window);
      }
    }
    uint32_t now = SDL_GetTicks();
    double dt = (now - timer.last_frame_time) / 1000.0;
    timer.last_frame_time = now;

    if (!timer.paused) {
      if (timer.is_shaking) {
        SDL_SetWindowPosition(window, timer.base_x, timer.base_y);
        timer.is_shaking = false;
      }
      timer.pause_duration = 0;
      timer.sec_remain -= dt;
      if (timer.state == w)
        timer.elapsed_work += dt;
      else
        timer.elapsed_break += dt;

      if (timer.sec_remain <= 0) {
        if (timer.state == w) {
          timer.state = b;
          timer.sec_remain = timer.config.break_min * 60.0;
        } else {
          timer.state = w;
          timer.sec_remain = timer.config.work_min * 60.0;
          timer.elapsed_work = 0;
          timer.elapsed_break = 0;
        }
      }
    } else {
      timer.pause_duration += dt;
      if (timer.pause_duration > 300.0) { // 5 minutes
        if (!timer.is_shaking) {
          SDL_GetWindowPosition(window, &timer.base_x, &timer.base_y);
          timer.is_shaking = true;
        }
        int offsetX = (rand() % 5) - 2;
        int offsetY = (rand() % 5) - 2;
        SDL_SetWindowPosition(window, timer.base_x + offsetX,
                              timer.base_y + offsetY);
      }
    }

    // sync audio
    if (timer.music && !timer.paused && timer.config.sound_on) {
      double target_pos;
      if (timer.state == w) {
        target_pos = fmod(timer.elapsed_work, 1500.0);
      } else {
        target_pos = 1500.0 + fmod(timer.elapsed_break, 300.0);
      }

      double music_pos = Mix_GetMusicPosition(timer.music);
      if (fabs(music_pos - target_pos) > 1.0) {
        Mix_SetMusicPosition(target_pos);
      }
    }

    glClearColor(0.10f, 0.12f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, ww, wh, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Solid background disc
    draw_filled_circle(ww / 2.0f, wh / 2.0f, 100, 0.15f, 0.17f, 0.25f, 1.0f);

    // back ringy
    draw_ring_segment(ww / 2.0f, wh / 2.0f, 80, 75, 0, 360, 0.15f, 0.17f, 0.25f,
                      1.0f);

    // progress
    double total = (timer.state == w) ? timer.config.work_min * 60.0
                                      : timer.config.break_min * 60.0;

    // Draw settings if open
    if (timer.settings_win) {
      render_settings(&timer, font_small);
    }
    float progress = (float)(timer.sec_remain / total);
    if (progress < 0)
      progress = 0;

    float ring_r = (timer.state == w) ? 1.0f : 0.4f;
    float ring_g = (timer.state == w) ? 0.45f : 0.85f;
    float ring_b = (timer.state == w) ? 0.45f : 1.0f;
    draw_ring_segment(ww / 2.0f, wh / 2.0f, 80, 75, 0, 360.0f * progress,
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
        timer.paused ? "PAUSED"
                     : (timer.state == w ? "GOOD BOY :3" : "BREAK! ENJOY");
    SDL_Color gray = {191, 199, 230, 230};
    update_cached_text(font_small, &label_cache, label, gray);
    draw_cached_text(&label_cache, ww / 2.0f, wh / 2.0f + 25, true, 1.0f);
    SDL_GL_SwapWindow(window);
  }
  if (time_cache.texture)
    glDeleteTextures(1, &time_cache.texture);
  if (label_cache.texture)
    glDeleteTextures(1, &label_cache.texture);
  if (timer.music) {
    Mix_FreeMusic(timer.music);
  }
  Mix_CloseAudio();
  TTF_CloseFont(font_large);
  TTF_CloseFont(font_small);
  TTF_Quit();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}