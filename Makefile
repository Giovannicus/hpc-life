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
	rm -f life_serial life_prof life_omp life_mpi life_mpi_2d life_mpi_2d_overlap gmon.out

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

life_mpi: src/life_mpi.c
	mpicc $(CFLAGS) -o life_mpi src/life_mpi.c

multi_process: life_mpi
	mpirun --use-hwthread-cpus -np $(THREADS) ./life_mpi $(N) $(STEPS)

life_mpi_2d: src/life_mpi_2d.c
	mpicc $(CFLAGS) -o life_mpi_2d src/life_mpi_2d.c

multi_process_2d: life_mpi_2d
	mpirun --use-hwthread-cpus -np $(THREADS) ./life_mpi_2d $(N) $(STEPS)

life_mpi_2d_overlap: src/life_mpi_2d_overlap.c
	mpicc $(CFLAGS) -o life_mpi_2d_overlap src/life_mpi_2d_overlap.c

multi_process_2d_overlap: life_mpi_2d_overlap
	mpirun --use-hwthread-cpus -np $(THREADS) ./life_mpi_2d_overlap $(N) $(STEPS)

compare: life_serial life_omp life_mpi
	./life_serial $(N) $(STEPS)
	OMP_NUM_THREADS=$(THREADS) ./life_omp $(N) $(STEPS)
	mpirun --use-hwthread-cpus -np $(THREADS) ./life_mpi $(N) $(STEPS)

.PHONY: all clean run_standard bench profile multi_thread multi_process multi_process_2d multi_process_2d_overlap compare