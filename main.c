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
#define STREAK_PATH "streak.txt"
#include <time.h>

typedef enum { w, b } State;

typedef struct {
  int work_min;
  int break_min;
  int long_break_min;
  int sessions_until_long;
  bool sound_on;
  bool auto_start;
  int opacity; // 0-100
  int volume;  // 0-128
  int x, y;
} Config;

typedef struct {
  char last_date[11]; // YYYY-MM-DD
  int daily_sessions;
  int consecutive_days;
} Streak;

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
  int session_count;
  int selected_setting;
  Streak streak;
  SDL_Window *streak_win;
  SDL_Renderer *streak_ren;
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
  fprintf(f, "long_break_time=%d\n", cfg->long_break_min);
  fprintf(f, "sessions_until_long=%d\n", cfg->sessions_until_long);
  fprintf(f, "sound=%d\n", cfg->sound_on ? 1 : 0);
  fprintf(f, "auto_start=%d\n", cfg->auto_start ? 1 : 0);
  fprintf(f, "opacity=%d\n", cfg->opacity);
  fprintf(f, "volume=%d\n", cfg->volume);
  fprintf(f, "x=%d\n", cfg->x);
  fprintf(f, "y=%d\n", cfg->y);
  fclose(f);
}

void load_config(Config *cfg) {
  cfg->work_min = 25;
  cfg->break_min = 5;
  cfg->long_break_min = 15;
  cfg->sessions_until_long = 4;
  cfg->sound_on = true;
  cfg->auto_start = false;
  cfg->opacity = 100;
  cfg->volume = 128;
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
    if (sscanf(line, "long_break_time=%d", &cfg->long_break_min) == 1)
      continue;
    if (sscanf(line, "sessions_until_long=%d", &cfg->sessions_until_long) == 1)
      continue;
    if (sscanf(line, "sound=%d", (int *)&cfg->sound_on) == 1)
      continue;
    if (sscanf(line, "auto_start=%d", (int *)&cfg->auto_start) == 1)
      continue;
    if (sscanf(line, "opacity=%d", &cfg->opacity) == 1)
      continue;
    if (sscanf(line, "volume=%d", &cfg->volume) == 1)
      continue;
    if (sscanf(line, "x=%d", &cfg->x) == 1)
      continue;
    if (sscanf(line, "y=%d", &cfg->y) == 1)
      continue;
  }
  fclose(f);
}

void save_streak(const Streak *s) {
  FILE *f = fopen(STREAK_PATH, "w");
  if (!f)
    return;
  fprintf(f, "last_date=%s\n", s->last_date);
  fprintf(f, "daily_sessions=%d\n", s->daily_sessions);
  fprintf(f, "consecutive_days=%d\n", s->consecutive_days);
  fclose(f);
}

void load_streak(Streak *s) {
  strcpy(s->last_date, "0000-00-00");
  s->daily_sessions = 0;
  s->consecutive_days = 0;

  FILE *f = fopen(STREAK_PATH, "r");
  if (!f)
    return;
  char line[128];
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "last_date=%10s", s->last_date) == 1)
      continue;
    if (sscanf(line, "daily_sessions=%d", &s->daily_sessions) == 1)
      continue;
    if (sscanf(line, "consecutive_days=%d", &s->consecutive_days) == 1)
      continue;
  }
  fclose(f);
}

void update_streak(Streak *s) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char today[11];
  strftime(today, sizeof(today), "%Y-%m-%d", t);

  if (strcmp(s->last_date, today) == 0) {
    s->daily_sessions++;
  } else {
    // Check if it was yesterday
    struct tm last_tm = {0};
    int y, m, d;
    if (sscanf(s->last_date, "%d-%d-%d", &y, &m, &d) == 3) {
      last_tm.tm_year = y - 1900;
      last_tm.tm_mon = m - 1;
      last_tm.tm_mday = d;
      last_tm.tm_isdst = -1;
      time_t last_t = mktime(&last_tm);

      double diff = difftime(now, last_t);
      if (diff > 0 && diff <= 86400 * 1.5) { // Roughly within a day
        s->consecutive_days++;
      } else {
        s->consecutive_days = 1;
      }
    } else {
      s->consecutive_days = 1;
    }
    s->daily_sessions = 1;
    strcpy(s->last_date, today);
  }
  save_streak(s);
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
      SDL_CreateWindow("Advanced Settings", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 400, 350, SDL_WINDOW_SHOWN);
  timer->settings_ren =
      SDL_CreateRenderer(timer->settings_win, -1, SDL_RENDERER_ACCELERATED);
  timer->selected_setting = 0;
}

void render_settings(Timer *timer, TTF_Font *font) {
  if (!timer->settings_win)
    return;

  SDL_SetRenderDrawColor(timer->settings_ren, 20, 22, 28, 255);
  SDL_RenderClear(timer->settings_ren);

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color yellow = {255, 215, 0, 255};
  char buf[128];

  const char *settings_names[] = {"Work Duration (min)",
                                  "Break Duration (min)",
                                  "Long Break Duration (min)",
                                  "Sessions until Long Break",
                                  "Sound Enabled",
                                  "Auto-start Next Session",
                                  "Opacity (0-100)",
                                  "Volume (0-128)"};

  int num_settings = 8;

  for (int i = 0; i < num_settings; i++) {
    SDL_Color color = (timer->selected_setting == i) ? yellow : white;
    switch (i) {
    case 0:
      sprintf(buf, "%s: %d", settings_names[i], timer->config.work_min);
      break;
    case 1:
      sprintf(buf, "%s: %d", settings_names[i], timer->config.break_min);
      break;
    case 2:
      sprintf(buf, "%s: %d", settings_names[i], timer->config.long_break_min);
      break;
    case 3:
      sprintf(buf, "%s: %d", settings_names[i],
              timer->config.sessions_until_long);
      break;
    case 4:
      sprintf(buf, "%s: %s", settings_names[i],
              timer->config.sound_on ? "ON" : "OFF");
      break;
    case 5:
      sprintf(buf, "%s: %s", settings_names[i],
              timer->config.auto_start ? "YES" : "NO");
      break;
    case 6:
      sprintf(buf, "%s: %d%%", settings_names[i], timer->config.opacity);
      break;
    case 7:
      sprintf(buf, "%s: %d", settings_names[i], timer->config.volume);
      break;
    }

    SDL_Surface *s = TTF_RenderText_Blended(font, buf, color);
    SDL_Texture *t = SDL_CreateTextureFromSurface(timer->settings_ren, s);
    SDL_Rect r = {20, 20 + i * 40, s->w, s->h};
    SDL_RenderCopy(timer->settings_ren, t, NULL, &r);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
  }

  SDL_RenderPresent(timer->settings_ren);
}

void open_streak_window(Timer *timer, TTF_Font *font) {
  if (timer->streak_win)
    return;
  timer->streak_win =
      SDL_CreateWindow("Streak Counter", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 300, 200, SDL_WINDOW_SHOWN);
  timer->streak_ren =
      SDL_CreateRenderer(timer->streak_win, -1, SDL_RENDERER_ACCELERATED);
}

void render_streak(Timer *timer, TTF_Font *font) {
  if (!timer->streak_win)
    return;
  SDL_SetRenderDrawColor(timer->streak_ren, 20, 22, 28, 255);
  SDL_RenderClear(timer->streak_ren);
  SDL_Color white = {255, 255, 255, 255};
  char buf[128];

  sprintf(buf, "Daily Sessions: %d", timer->streak.daily_sessions);
  SDL_Surface *s1 = TTF_RenderText_Blended(font, buf, white);
  SDL_Texture *t1 = SDL_CreateTextureFromSurface(timer->streak_ren, s1);
  SDL_Rect r1 = {20, 20, s1->w, s1->h};
  SDL_RenderCopy(timer->streak_ren, t1, NULL, &r1);
  SDL_FreeSurface(s1);
  SDL_DestroyTexture(t1);

  sprintf(buf, "Consecutive Days: %d", timer->streak.consecutive_days);
  SDL_Surface *s2 = TTF_RenderText_Blended(font, buf, white);
  SDL_Texture *t2 = SDL_CreateTextureFromSurface(timer->streak_ren, s2);
  SDL_Rect r2 = {20, 60, s2->w, s2->h};
  SDL_RenderCopy(timer->streak_ren, t2, NULL, &r2);
  SDL_FreeSurface(s2);
  SDL_DestroyTexture(t2);

  sprintf(buf, "Last Active: %s", timer->streak.last_date);
  SDL_Surface *s3 = TTF_RenderText_Blended(font, buf, white);
  SDL_Texture *t3 = SDL_CreateTextureFromSurface(timer->streak_ren, s3);
  SDL_Rect r3 = {20, 100, s3->w, s3->h};
  SDL_RenderCopy(timer->streak_ren, t3, NULL, &r3);
  SDL_FreeSurface(s3);
  SDL_DestroyTexture(t3);

  SDL_RenderPresent(timer->streak_ren);
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
  load_streak(&timer.streak);
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
  SDL_SetWindowOpacity(window, timer.config.opacity / 100.0f);

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
        } else if (timer.streak_win &&
                   e.window.windowID == SDL_GetWindowID(timer.streak_win)) {
          SDL_DestroyRenderer(timer.streak_ren);
          SDL_DestroyWindow(timer.streak_win);
          timer.streak_win = NULL;
          timer.streak_ren = NULL;
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
        } else if (e.key.keysym.sym == SDLK_o) {
          if (!timer.streak_win)
            open_streak_window(&timer, font_small);
          else {
            SDL_DestroyRenderer(timer.streak_ren);
            SDL_DestroyWindow(timer.streak_win);
            timer.streak_win = NULL;
            timer.streak_ren = NULL;
          }
        }

        if (timer.settings_win) {
          if (e.key.keysym.sym == SDLK_UP) {
            timer.selected_setting = (timer.selected_setting - 1 + 8) % 8;
          } else if (e.key.keysym.sym == SDLK_DOWN) {
            timer.selected_setting = (timer.selected_setting + 1) % 8;
          } else if (e.key.keysym.sym == SDLK_LEFT ||
                     e.key.keysym.sym == SDLK_RIGHT) {
            int dir = (e.key.keysym.sym == SDLK_RIGHT) ? 1 : -1;
            switch (timer.selected_setting) {
            case 0:
              timer.config.work_min = fmax(1, timer.config.work_min + dir);
              break;
            case 1:
              timer.config.break_min = fmax(1, timer.config.break_min + dir);
              break;
            case 2:
              timer.config.long_break_min =
                  fmax(1, timer.config.long_break_min + dir);
              break;
            case 3:
              timer.config.sessions_until_long =
                  fmax(1, timer.config.sessions_until_long + dir);
              break;
            case 4:
              timer.config.sound_on = !timer.config.sound_on;
              break;
            case 5:
              timer.config.auto_start = !timer.config.auto_start;
              break;
            case 6:
              timer.config.opacity =
                  fmax(10, fmin(100, timer.config.opacity + dir * 5));
              break;
            case 7:
              timer.config.volume =
                  fmax(0, fmin(128, timer.config.volume + dir * 8));
              break;
            }
            if (timer.selected_setting == 6) {
              SDL_SetWindowOpacity(window, timer.config.opacity / 100.0f);
            }
            if (timer.selected_setting == 4 || timer.selected_setting == 7) {
              Mix_VolumeMusic(timer.config.volume);
              if (timer.config.sound_on)
                Mix_ResumeMusic();
              else
                Mix_HaltMusic();
            }
            save_config(&timer.config);
          }
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
          timer.session_count++;
          update_streak(&timer.streak);
          if (timer.session_count % timer.config.sessions_until_long == 0) {
            timer.state = b;
            timer.sec_remain = timer.config.long_break_min * 60.0;
          } else {
            timer.state = b;
            timer.sec_remain = timer.config.break_min * 60.0;
          }
        } else {
          timer.state = w;
          timer.sec_remain = timer.config.work_min * 60.0;
          timer.elapsed_work = 0;
          timer.elapsed_break = 0;
        }
        if (!timer.config.auto_start) {
          timer.paused = true;
          Mix_PauseMusic();
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
    if (timer.streak_win) {
      render_streak(&timer, font_small);
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
    char label[64];
    if (timer.paused) {
      strcpy(label, "PAUSED");
    } else if (timer.state == w) {
      sprintf(label, "WORK #%d", timer.session_count + 1);
    } else {
      strcpy(label, "BREAK! ENJOY");
    }
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