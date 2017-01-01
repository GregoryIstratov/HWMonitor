CC=clang

all:
	$(CC) -march=native -mtune=native -std=gnu11 -D_GNU_SOURCE -DNDEBUG -O3 -funroll-loops -Weverything -Werror -Wno-unused-function -Wno-unused-macros -Wno-unused-parameter -Wno-format-nonliteral -lncurses -lpthread -o HWMonitor -s main.c

debug:
	$(CC) -march=native -mtune=native -std=gnu11 -D_GNU_SOURCE -O0 -g -Weverything -Werror -Wno-unused-function -Wno-unused-macros -Wno-unused-parameter -Wno-format-nonliteral -lncurses -lpthread -o HWMonitor main.c

clean:
	rm HWMonitor
