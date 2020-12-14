#!/usr/bin/env bash

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd "$script_dir"

gcc -Wall -Werror -g -O0 -I${HMALLOC_ENV_DIR}/include -lm -o accesses src/accesses.c
