CC = gcc
CFLAGS = -Wall -g
LIBS = -framework ApplicationServices

all: process_manager

process_manager: main.o process_manager.o threadFinder.o
	$(CC) $(CFLAGS) -o process_manager main.o process_manager.o threadFinder.o $(LIBS)

main.o: main.c process_manager.h
	$(CC) $(CFLAGS) -c main.c

process_manager.o: process_manager.c process_manager.h
	$(CC) $(CFLAGS) -c process_manager.c

threadFinder.o: threadFinder.c
	$(CC) $(CFLAGS) -c threadFinder.c

clean:
	rm -f main.o process_manager.o threadFinder.o process_manager *~ \#*\#

.PHONY: clean all 