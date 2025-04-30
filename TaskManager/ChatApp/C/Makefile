CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -pthread -I/opt/homebrew/opt/openssl@3/include
LDFLAGS = -pthread -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto

# Output directory
BIN_DIR = bin

# Source files
SERVER_SRC = chat_server.c
CLIENT_SRC = chat_client.c

# Output files
SERVER = $(BIN_DIR)/chat_server
CLIENT = $(BIN_DIR)/chat_client

all: setup $(SERVER) $(CLIENT)

setup:
	@mkdir -p $(BIN_DIR)
	@mkdir -p uploads

$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR) uploads *.o

.PHONY: all clean setup 