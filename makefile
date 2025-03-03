CC = gcc
CFLAGS = -Wall -Wextra -Os -flto -march=native -DNDEBUG
LDFLAGS = -levdev

SRCS = numlockwl.c
OBJ = $(SRCS:.c=.o)
TARGET = numlockwl

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -I/usr/include/libevdev-1.0 -I/usr/include/libevdev-1.0/libevdev $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
