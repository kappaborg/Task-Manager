CC = gcc
CFLAGS = -Wall -Wextra -I/opt/homebrew/opt/openssl@3/include
LDFLAGS = -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -pthread

SRCDIR = .
BINDIR = bin
OBJDIR = obj

SERVER_SRCS = chat_server.c security.c ui_components.c
CLIENT_SRCS = chat_client.c security.c ui_components.c
SERVER_OBJS = $(SERVER_SRCS:%.c=$(OBJDIR)/%.o)
CLIENT_OBJS = $(CLIENT_SRCS:%.c=$(OBJDIR)/%.o)

SERVER = $(BINDIR)/chat_server
CLIENT = $(BINDIR)/chat_client

.PHONY: all clean

all: $(SERVER) $(CLIENT)

$(SERVER): $(SERVER_OBJS) | $(BINDIR)
	$(CC) $(SERVER_OBJS) -o $@ $(LDFLAGS)

$(CLIENT): $(CLIENT_OBJS) | $(BINDIR)
	$(CC) $(CLIENT_OBJS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR) $(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(BINDIR) $(OBJDIR) 