CC=clang

all:
	$(CC) -march=native -mtune=native -std=c11 -O3 -funroll-loops -Wl,--gc-sections -Wl,--strip-all -s -D_GNU_SOURCE -DNDEBUG -lncurses -lpthread -o HWMonitor main.c

debug:
	$(CC) -march=native -mtune=native -std=c11 -O0 -g -D_GNU_SOURCE -lncurses -lpthread -o HWMonitor main.c

clean:
	rm HWMonitor
