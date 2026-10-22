#!/bin/bash

# Runs every version in ONE session (same machine state), so that the times
# are comparable. Results are appended to results/benchmarks.csv with
# TAPPA=final (bench.sh prints them too).
#
# usage: ./scripts/final_comparison.sh
#        SIZES="1024 4096" ./scripts/final_comparison.sh    (fewer sizes, faster)
#
# Run it with the machine idle: no other heavy program while it is running.
cd "$(dirname "$0")/.." || exit 1

make life_serial life_omp life_mpi life_mpi_2d life_mpi_2d_overlap life_hybrid || exit 1

echo "load average before starting: $(cut -d' ' -f1-3 /proc/loadavg)"
export TAPPA=final

# same launcher for every MPI run: no binding, so that threads are never
# confined to one core (see README, section Tappa 6)
launcher() {
    echo "mpirun --use-hwthread-cpus --bind-to none -np $1"
}

echo "### serial"
PROCS=1 ./scripts/bench.sh ./life_serial 1

for t in 1 2 4 8; do
    echo "### OpenMP, $t threads"
    PROCS=1 ./scripts/bench.sh ./life_omp $t
done

for prog in life_mpi life_mpi_2d life_mpi_2d_overlap; do
    for p in 1 2 4 8; do
        echo "### $prog, $p processes"
        PROCS=$p LAUNCHER="$(launcher $p)" ./scripts/bench.sh ./$prog 1
    done
done

# processes x threads
for cfg in "1 1" "2 1" "1 2" "4 1" "2 2" "1 4" "8 1" "4 2" "2 4" "1 8"; do
    set -- $cfg
    echo "### life_hybrid, $1 processes x $2 threads"
    PROCS=$1 LAUNCHER="$(launcher $1)" ./scripts/bench.sh ./life_hybrid $2
done

echo "load average at the end: $(cut -d' ' -f1-3 /proc/loadavg)"
echo "### FINISHED"
