CC      = gcc
OBJC    = clang

TARGET      = pomopomo
TARGET_MAC  = pomopomo_mac

SRC_SDL = main.c
SRC_MAC = main.m

# detect if its mac or linux (no windows it sucks)
UNAME_S := $(shell uname -s)

#SDL flags (cross-platform via pkg-config)
CFLAGS_SDL = $(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_mixer)
LIBS_SDL   = $(shell pkg-config --libs sdl2 SDL2_ttf SDL2_mixer)

ifeq ($(UNAME_S),Darwin)
    # macOS specific
    LIBS_SDL += -framework OpenGL -framework ApplicationServices -framework CoreGraphics
    CFLAGS_MAC =
    LIBS_MAC   = -framework Cocoa -framework QuartzCore -framework Metal -framework OpenGL -framework ApplicationServices -framework AVFoundation
else
    # Linux specific
    LIBS_SDL += -lGL -lm
endif

$(TARGET): $(SRC_SDL)
	$(CC) $(CFLAGS_SDL) -o $(TARGET) $(SRC_SDL) $(LIBS_SDL)

# macOS only target
mac: $(SRC_MAC)
ifeq ($(UNAME_S),Darwin)
	$(OBJC) -ObjC -o $(TARGET_MAC) $(SRC_MAC) $(LIBS_MAC)
else
	@echo "macOS target is only available on Darwin"
endif

clean:
	rm -f $(TARGET) $(TARGET_MAC)

.PHONY: clean mac
