# Host-side unit tests (no hardware needed): run `make` in this folder.
CC      ?= gcc
CFLAGS  += -std=c99 -Wall -Wextra -I../src -I.
LOGIC    = ../src/midi.c ../src/switches.c

all: test_midi test_switches test_app
	./test_midi && ./test_switches && ./test_app

test_midi: test_midi.c $(LOGIC)
	$(CC) $(CFLAGS) -o $@ $^

test_switches: test_switches.c $(LOGIC)
	$(CC) $(CFLAGS) -o $@ $^

test_app: test_app.c mock_hal.c ../src/app.c $(LOGIC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f test_midi test_switches test_app
