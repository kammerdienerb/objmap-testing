#!/usr/bin/env bash

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd "$script_dir"

for d in $(find . -mindepth 1 -maxdepth 1 -type d); do
    echo ${d}
    (cd ${d}; ./build.sh)
done
