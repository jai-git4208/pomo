CC      = gcc
OBJC    = clang

TARGET      = pomopomo
TARGET_MAC  = pomopomo_mac

SRC_SDL = main.c
SRC_MAC = main.m

CFLAGS_SDL = $(shell pkg-config --cflags sdl2 SDL2_ttf)
LIBS_SDL   = $(shell pkg-config --libs sdl2 SDL2_ttf) -framework OpenGL

CFLAGS_MAC =
LIBS_MAC   = -framework Cocoa -framework QuartzCore -framework Metal -framework OpenGL

$(TARGET): $(SRC_SDL)
	$(CC) $(CFLAGS_SDL) -o $(TARGET) $(SRC_SDL) $(LIBS_SDL)

mac: $(SRC_MAC)
	$(OBJC) -ObjC -o $(TARGET_MAC) $(SRC_MAC) $(LIBS_MAC)

clean:
	rm -f $(TARGET) $(TARGET_MAC)

