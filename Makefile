.phony: test all clean

test: binary
	./binary

all: binary

clean:
	rm -f binary

binary: main.cc shared_between_threads.h
	g++ -std=c++11 -o $@ $< -lpthread

