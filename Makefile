CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -O2
DEBUG_CFLAGS = -Wall -Wextra -pedantic -std=c99 -g -DDEBUG
PROCESS_MANAGER = process_manager
SRCS = main.c process_manager.c
OBJS = $(SRCS:.c=.o)
LDFLAGS = -pthread
all: $(PROCESS_MANAGER) chat_app

debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(PROCESS_MANAGER)

$(PROCESS_MANAGER): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

chat_app:
	$(MAKE) -C "Chat App"

clean:
	rm -f $(OBJS) $(PROCESS_MANAGER) *~ \#*\#
	$(MAKE) -C "Chat App" clean

install: $(PROCESS_MANAGER)
	mkdir -p $(HOME)/bin
	cp $(PROCESS_MANAGER) $(HOME)/bin/

uninstall:
	rm -f $(HOME)/bin/$(PROCESS_MANAGER)

.PHONY: all debug clean install uninstall chat_app 