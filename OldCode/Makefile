CXX = gcc
CXXFLAGS = -O2 -lftdi1 -lncurses -I/usr/include/libftdi1

broadcast: broadcast.c
	$(CXX) $^ $(CXXFLAGS) -o $@

install-packages:
	sudo pacman -S libftdi

clean:
	rm -f broadcast