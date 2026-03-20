TARGET := cpv_convert

CC := gcc
CFLAGS := -I. -Wall -Wextra -O2
LDFLAGS := -static

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
