CC = gcc
CFLAGS = -O2 -Wall
PROFILE = -pg -g -fno-inline
OPEN_OMP = -fopenmp
N ?= 1024
STEPS ?= 100
THREADS ?= 8

all: life_serial

life_serial: src/life_serial.c
	$(CC) $(CFLAGS) -o life_serial src/life_serial.c

clean:
	rm -f life_serial life_prof life_omp gmon.out

run_standard: life_serial
	./life_serial $(N) $(STEPS)

bench: life_serial
	./scripts/bench.sh

life_prof: src/life_serial.c
	$(CC) $(CFLAGS) $(PROFILE) -o life_prof src/life_serial.c

profile: life_prof
	./life_prof $(N) $(STEPS)
	gprof ./life_prof gmon.out | head -20

life_omp: src/life_omp.c
	$(CC) $(CFLAGS) $(OPEN_OMP) -o life_omp src/life_omp.c

multi_thread: life_omp
	OMP_NUM_THREADS=$(THREADS) ./life_omp $(N) $(STEPS)

.PHONY: all clean run_standard bench profile multi_thread