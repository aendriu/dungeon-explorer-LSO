CC=gcc
CFLAGS=-Wall -Wextra -O2 -g -pthread

SERVER_OUT=server.out
SERVER_SRC=server/src/*.c
SERVER_INC=-Iserver

CLIENT_OUT=client.out
CLIENT_SRC=client/src/*.c
CLIENT_INC=-Iclient

all: $(SERVER_OUT) $(CLIENT_OUT)

$(SERVER_OUT):
	$(CC) $(CFLAGS) $(SERVER_INC) -o $(SERVER_OUT) $(SERVER_SRC)

$(CLIENT_OUT):
	$(CC) -Wall -Wextra -O2 -g $(CLIENT_INC) -o $(CLIENT_OUT) $(CLIENT_SRC)

clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)

.PHONY: all clean