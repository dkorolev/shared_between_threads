.phony: test all clean indent

test: binary
	./binary

all: binary

clean:
	rm -f binary

binary: main.cc shared_between_threads.h
	g++ -std=c++11 -o $@ $< -lpthread

indent:
	find . -regextype posix-egrep -regex ".*\.(cc|h)" | xargs clang-format-3.5 -i
