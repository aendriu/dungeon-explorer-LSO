CC=gcc
CFLAGS=-Wall -Wextra -O2 -g -pthread

SERVER_OUT=server.out
SERVER_SRC=$(wildcard server/src/*.c)
SERVER_HDR=$(wildcard server/header/*.h)
SERVER_INC=-Iserver -Iutils/cjson

CLIENT_OUT=client.out
CLIENT_SRC=$(wildcard client/src/*.c)
CLIENT_HDR=$(wildcard client/header/*.h)
CLIENT_INC=-Iclient -Iutils/cjson

CJSON_SRC=utils/cjson/cJSON.c
UTILS_HDR=$(wildcard utils/*.h)

all: $(SERVER_OUT) $(CLIENT_OUT)

$(SERVER_OUT): $(SERVER_SRC) $(SERVER_HDR) $(CJSON_SRC) $(UTILS_HDR)
	$(CC) $(CFLAGS) $(SERVER_INC) -o $(SERVER_OUT) $(SERVER_SRC) $(CJSON_SRC) -lm

$(CLIENT_OUT): $(CLIENT_SRC) $(CLIENT_HDR) $(CJSON_SRC) $(UTILS_HDR)
	$(CC) $(CFLAGS) $(CLIENT_INC) -o $(CLIENT_OUT) $(CLIENT_SRC) $(CJSON_SRC) -lncurses -lm

clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)

.PHONY: all clean