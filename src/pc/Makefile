TARGET := cpv_convert cpv_player

CC := gcc
CFLAGS := -I. -Wall -Wextra -O2
LDFLAGS_STATIC := -static
SDL_FLAGS := $(shell sdl2-config --cflags --libs)

MAIN_SOURCES := main.c
MAIN_OBJECTS := $(MAIN_SOURCES:.c=.o)
PLAYER_SOURCES := player.c
PLAYER_OBJECTS := $(PLAYER_SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

cpv_convert: $(MAIN_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_STATIC)

cpv_player: $(PLAYER_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(SDL_FLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o