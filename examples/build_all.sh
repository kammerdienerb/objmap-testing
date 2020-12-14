#!/usr/bin/env bash

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd "$script_dir"

for d in $(find . -type d); do
    if [ "$d" = "." ] || [ "$d" = ".." ]; then
        continue
    fi
    (cd ${d}; ./build.sh)
done
