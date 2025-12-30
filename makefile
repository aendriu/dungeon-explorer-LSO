CC=gcc
CFLAGS=-Wall -Wextra -O2 -g -pthread

TARGET=server.out
SRC=server/src/*.c
INC=-Iserver

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(INC) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean