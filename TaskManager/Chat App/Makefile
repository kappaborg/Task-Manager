CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=2
LDFLAGS = -pthread
NCURSES_FLAGS = -lncurses
TARGETS = server client client_tui

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

client_tui: client_tui.c
	$(CC) $(CFLAGS) -o client_tui client_tui.c $(LDFLAGS) $(NCURSES_FLAGS)

debug: CFLAGS += -g -DDEBUG
debug: clean all

clean:
	rm -f $(TARGETS)
	rm -f /tmp/chat_server_fifo
	rm -f /tmp/chat_client_*_fifo

install:
	mkdir -p $(HOME)/bin
	cp $(TARGETS) $(HOME)/bin/

.PHONY: all clean debug install
