LIBRARY=nope
INCPATHS=include
LIBPATHS=.
CPPFLAGS=-std=c++11 -Wall -Wextra -g
CC=g++

build: server subscriber

server: server.o lib.o
	$(CC) $^ -o $@

server.o: server.cpp
	$(CC) $(CPPFLAGS) $(foreach TMP,$(INCPATHS),-I$(TMP)) -c $< -o $@

subscriber: subscriber.o lib.o
	$(CC) $^ -o $@

subscriber.o: subscriber.cpp
	$(CC) $(CPPFLAGS) $(foreach TMP,$(INCPATHS),-I$(TMP)) -c $< -o $@

lib.o: lib/lib.cpp
	$(CC) $(CPPFLAGS) $(foreach TMP,$(INCPATHS),-I$(TMP)) -c $< -o $@

clean:
	rm -f *.o server subscriber