#!/usr/bin/env bash

export BENCH_DIR="benchmarks/lulesh"
export CONFIG="configs/sites_tiny.sh"
# export TOOL="examples/live_largest_sites/live_largest_sites %p 100"
# export TOOL="examples/accesses/accesses %p 1000"
export RUN_TOOL_AS_ROOT="yes"

bench_run.sh
