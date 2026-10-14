#!/bin/bash

prog=${1:-./life_serial}
threads=${2:-1}

for n in 512 1024 2048 4096; do
times=""
ref=""
    for r in 1 2 3 4 5; do
        output=$(OMP_NUM_THREADS=$threads $prog $n 100)

        v=$(echo "$output" | awk '{print $4}' | sed 's/vive=//')
        if [ -z "$ref" ]; then
            ref=$v
        elif [ "$v" != "$ref" ]; then
            echo "ATTENZIONE: n=$n vive diverso ($v invece di $ref)" >&2
        fi

        t=$(echo "$output" | awk '{print $3}' | sed 's/tempo=//; s/s$//')
        times="$times$t"$'\n'
    done
    min=$(echo "$times" | grep . | sort -n | head -1)
    echo "n=$n threads=${threads:-1} tempo_min=$min vive=$ref"
done