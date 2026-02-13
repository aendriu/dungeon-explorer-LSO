CC=gcc
CFLAGS=-Wall -Wextra -O2 -g -pthread

SERVER_OUT=server.out
SERVER_SRC=server/src/*.c
SERVER_INC=-Iserver -Iutils/cjson

CLIENT_OUT=client.out
CLIENT_SRC=client/src/*.c
CLIENT_INC=-Iclient -Iutils/cjson

CJSON_SRC=utils/cjson/cJSON.c

all: $(SERVER_OUT) $(CLIENT_OUT)

$(SERVER_OUT):
	$(CC) $(CFLAGS) $(SERVER_INC) -o $(SERVER_OUT) $(SERVER_SRC) $(CJSON_SRC) -lm

$(CLIENT_OUT):
	$(CC) -Wall -Wextra -O2 -g $(CLIENT_INC) -o $(CLIENT_OUT) $(CLIENT_SRC) $(CJSON_SRC) -lncurses -lm

clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)

.PHONY: all clean