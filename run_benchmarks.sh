#!/usr/bin/env bash
set -eu

mkdir -p results
stamp=$(date +%Y%m%d_%H%M%S)
outfile="results/bench_${stamp}.txt"

echo "Writing benchmark output to ${outfile}"

run_case() {
    echo "===== $* =====" | tee -a "$outfile"
    "$@" 2>&1 | tee -a "$outfile"
    echo | tee -a "$outfile"
}

if [ -x ./stencil_serial ]; then
    run_case ./stencil_serial 8192 8192 1000
fi

if [ -x ./stencil_omp ]; then
    #for t in 1 2 4 8 16; do
        #run_case ./stencil_omp 8192 8192 300 "$t"
    #done
    run_case ./stencil_omp 8192 8192 1000 16 
fi

if [ -x ./stencil_acc ]; then
    run_case ./stencil_acc 8192 8192 1000
fi

if [ -x ./stencil_cuda ]; then
    run_case ./stencil_cuda 8192 8192 1000 16 16
fi

if [ -x ./my_stencil_omp ]; then
    run_case ./my_stencil_omp 8192 8192 1000 16
fi

if [ -x ./my_stencil_acc ]; then
    run_case ./my_stencil_acc 8192 8192 1000
fi
