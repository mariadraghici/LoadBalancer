CC=gcc
CFLAGS=-Wall -Wextra -std=c99

.PHONY: build clean

build: tema2

tema2: main.o load_balancer.o server.o Hashtable.o LinkedList.o
	$(CC) $(CFLAGS) main.o load_balancer.o server.o Hashtable.o LinkedList.o -o tema2

load_balancer.o: load_balancer.h load_balancer.c
	$(CC) $(CFLAGS) load_balancer.c -c -o load_balancer.o

server.o: server.h server.c
	$(CC) $(CFLAGS) server.c -c -o server.o

Hashtable.o: Hashtable.h Hashtable.c
	$(CC) $(CFLAGS) Hashtable.c -c -o Hashtable.o

LinkedList.o: LinkedList.h LinkedList.c utils.h
	$(CC) $(CFLAGS) LinkedList.c -c -o LinkedList.o

main.o: main.c
	$(CC) $(CFLAGS) main.c -c -o main.o

clean:
	rm -f *.o tema2 *.h.gch
