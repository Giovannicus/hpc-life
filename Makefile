CC = gcc
CFLAGS = -O2 -Wall
PROFILE = -pg -g -fno-inline
N ?= 1024
STEPS ?= 100

all: life_serial

life_serial: src/life_serial.c
	$(CC) $(CFLAGS) -o life_serial src/life_serial.c

clean:
	rm -f life_serial life_prof gmon.out

run_standard: life_serial
	./life_serial $(N) $(STEPS)

bench: life_serial
	./scripts/bench.sh

life_prof: src/life_serial.c
	$(CC) $(CFLAGS) $(PROFILE) -o life_prof src/life_serial.c

profile: life_prof
	./life_prof $(N) $(STEPS)
	gprof ./life_prof gmon.out | head -20

.PHONY: all clean run_standard bench profile