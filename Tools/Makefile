CXX = gcc
CXXFLAGS = -O2 -lftdi1 -lncurses -I/usr/include/libftdi1

all: console

console: console.c
	$(CXX) $^ $(CXXFLAGS) -o $@

clean:
	rm -f console