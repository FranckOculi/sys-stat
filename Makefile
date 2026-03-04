CFLAGS ?= -Wall -Wextra -I include

all: systat

systat: main.o system.o network.o common.o
	gcc $(CFLAGS) -o $@ $^

printstat: printstat.o system.o common.o
	gcc $(CFLAGS) -o $@ $^

client_test: client_test.o system.o common.o
	gcc $(CFLAGS) -o $@ $^

%.o: src/%.c
	gcc $(CFLAGS) -o $@ -c $<

clean:
	rm -rf *.o

.PHONY: all clean