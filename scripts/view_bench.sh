#!/bin/bash

# Time per frame of the hybrid viewer (compute / gather / draw), in ms.
# For every n and every processes x threads config: 3 runs of 200 steps with
# no delay and automatic exit, median of the three. Results are appended to
# results/view_benchmarks.csv. A window opens and closes at every run.
# If it is interrupted, run it again: configs already measured today are skipped.
#
# usage: ./scripts/view_bench.sh
#        SIZES="256 512" ./scripts/view_bench.sh
cd "$(dirname "$0")/.." || exit 1

steps=200
sizes=${SIZES:-256 512 1024}
csv=results/view_benchmarks.csv
# SDL_VIDEODRIVER=dummy: no real window (stable, measures the drawing into memory);
# unset: real window through WSLg (includes presenting it to the desktop)
driver=${SDL_VIDEODRIVER:-real}

make life_serial life_view_hybrid || exit 1
mkdir -p results
if [ ! -f "$csv" ]; then
    echo "data,n,processi,thread,frames,compute_ms,gather_ms,draw_ms,driver" > "$csv"
fi

median() {   # median of the numbers read from stdin (3 values)
    sort -n | sed -n 2p
}

for n in $sizes; do
    ref=$(./life_serial $n $steps | awk '{print $4}' | sed 's/vive=//')
    # processes threads
    for cfg in "1 1" "4 1" "2 2" "1 4" "8 1" "4 2" "2 4" "1 8"; do
        set -- $cfg
        np=$1; t=$2
        # resume: skip the configs already measured today
        if grep -q "^$(date +%F),$n,$np,$t,.*,$driver\$" "$csv"; then
            echo "skip n=$n procs=$np threads=$t driver=$driver (already in $csv)"
            continue
        fi
        comp=""; gath=""; draw=""; frames=""
        for r in 1 2 3; do
            out=$(OMP_NUM_THREADS=$t mpirun --use-hwthread-cpus --bind-to none -np $np \
                  ./life_view_hybrid $n $steps 1 0 0 2>/dev/null | grep 'per frame')
            alive=$(echo "$out" | sed 's/.*alive=\([0-9]*\).*/\1/')
            if [ "$alive" != "$ref" ]; then
                echo "WARNING: n=$n np=$np threads=$t alive=$alive instead of $ref" >&2
            fi
            frames=$(echo "$out" | sed 's/.*frames=\([0-9]*\).*/\1/')
            comp="$comp$(echo "$out" | sed 's/.*compute=\([0-9.]*\).*/\1/')"$'\n'
            gath="$gath$(echo "$out" | sed 's/.*gather=\([0-9.]*\).*/\1/')"$'\n'
            draw="$draw$(echo "$out" | sed 's/.*draw=\([0-9.]*\).*/\1/')"$'\n'
        done
        c=$(echo "$comp" | grep . | median)
        g=$(echo "$gath" | grep . | median)
        d=$(echo "$draw" | grep . | median)
        echo "n=$n procs=$np threads=$t frames=$frames compute=$c gather=$g draw=$d (ms per frame)"
        echo "$(date +%F),$n,$np,$t,$frames,$c,$g,$d,$driver" >> "$csv"
    done
done
echo "FINISHED"
