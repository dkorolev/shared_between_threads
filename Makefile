CPP=clang++ -O3  # -g -DDEBUG
TARGETS=example1 example2 example3 example4

.PHONY: all clean indent

all: example1 example2 example3 example4

clean:
	rm -f ${TARGETS}

%: %.cc
	${CPP} -std=c++11 -o $@ $< -lpthread

indent:
	find . -regextype posix-egrep -regex ".*\.(cc|h)" | xargs clang-format-3.5 -i
