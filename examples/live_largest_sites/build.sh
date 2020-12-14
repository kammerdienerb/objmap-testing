#!/usr/bin/env bash

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd "$script_dir"

gcc -g -O0 -I${OBJMAP_ENV_DIR}/include -lm -o live_largest_sites src/live_largest_sites.c
