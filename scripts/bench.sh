#!/bin/bash

# usage: [TAPPA=4] [PROCS=4] [SIZES="512 1024"] [LAUNCHER="mpirun --use-hwthread-cpus -np 4"] ./scripts/bench.sh [program] [omp_threads]
# SIZES: list of grid sizes n (default: 512 1024 2048 4096)
# every run is also appended to results/benchmarks.csv
prog=${1:-./life_serial}
threads=${2:-1}
launcher=${LAUNCHER:-}
tappa=${TAPPA:-}
procs=${PROCS:-1}
version=$(basename "$prog")
csv=results/benchmarks.csv

mkdir -p results
if [ ! -f "$csv" ]; then
    echo "data,tappa,versione,n,processi,thread,tempo_min,vive" > "$csv"
fi

for n in ${SIZES:-512 1024 2048 4096}; do
times=""
ref=""
    for r in 1 2 3 4 5; do
        output=$(OMP_NUM_THREADS=$threads $launcher $prog $n 100)

        v=$(echo "$output" | awk '{print $4}' | sed 's/vive=//')
        if [ -z "$ref" ]; then
            ref=$v
        elif [ "$v" != "$ref" ]; then
            echo "WARNING: n=$n alive count differs ($v instead of $ref)" >&2
        fi

        t=$(echo "$output" | awk '{print $3}' | sed 's/tempo=//; s/s$//')
        times="$times$t"$'\n'
    done
    min=$(echo "$times" | grep . | sort -n | head -1)
    echo "n=$n threads=${threads:-1} launcher=[$launcher] tempo_min=$min vive=$ref"
    echo "$(date +%F),$tappa,$version,$n,$procs,$threads,$min,$ref" >> "$csv"
done
