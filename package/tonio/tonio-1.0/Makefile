TARGET = tonio
LIBS = -lwiringPi -lm -liw $(shell pkg-config --libs libmicrohttpd libvlc alsa libconfuse)
CC = gcc
CFLAGS = -g -Werror $(shell pkg-config --cflags libmicrohttpd libvlc alsa libconfuse)

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard src/*.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) -o $@

clean:
	-rm -f src/*.o
	-rm -f $(TARGET)
