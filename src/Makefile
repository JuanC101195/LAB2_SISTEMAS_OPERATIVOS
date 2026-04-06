CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = wish

all: $(TARGET)

$(TARGET): wish.c
	$(CC) $(CFLAGS) -o $(TARGET) wish.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
