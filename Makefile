CC = gcc
CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_ttf)
LIBS = $(shell pkg-config --libs sdl2 SDL2_ttf) -framework OpenGL

TARGET = pomopomo
SRCS = main.c

$(TARGET):$(SRCS)
		$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	rm -f $(TARGET)
