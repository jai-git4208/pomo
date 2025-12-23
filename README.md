# pomopomo

A circular Pomodoro timer to track how much time you spent doing maths.

## Features

**Two Versions**: pomopomo for cross-platform(mac & linux) and native cocoa implementation.
**Aesthetic Design**: Transparent window desgin that with smooth animation.
**Streak Counter**: tracks how many pomopomos u succeeded and stores in streak.txt
**Focus Detection**: automatically pause the timer if you are not working(keyboard/mouse not moving).
**Advanced Settings**: open settings by pressing s
**Audio**: really good pomopomo background music

## Controls

- **Space bar**: pause / resume timer.
- **'r'**: Reset timer.
- **'s'**: Open Settings window.
- **'o'**: Open Streak Counter window.
- **Escape**: Quit application (only for Cocoa version).

## Compiling and Running

### SDL Version
requires SDL2, SDL2_ttf, SDL2_mixer, and OpenGL.
```bash
make clean && make
./pomopomo
```

### Cocoa Version
native macOS implementation.
```bash
make mac
./pomopomo_mac
```

## Configuration

settings are stored in `pomo.cfg` and include:
- `work_time`: Work session duration in minutes.
- `break_time`: Short break duration in minutes.
- `long_break_time`: Long break duration in minutes.
- `sessions_until_long`: Number of work sessions before a long break.
- `sound`: Toggle sound (1/0).
- `auto_start`: Toggle auto-starting the next session.
- `opacity`: Window opacity (10-100).
- `volume`: Sound volume (0-128).
- `focus_threshold`: Inactivity timeout in seconds.
- `x`, `y`: Last saved window position.

## MADE WITH LOVE BY JAIMIN
