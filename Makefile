CC=/bin/cc

all:
	$(CC) -march=native -mtune=native -std=c11 -O3 -funroll-loops -fuse-ld=gold -Wl,--gc-sections -Wl,--strip-all -s -D_GNU_SOURCE -DNDEBUG -lncurses -lpthread -o HWMonitor main.c

clean:
	rm HWMonitor
