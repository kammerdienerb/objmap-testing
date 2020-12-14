#!/usr/bin/env bash

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source ${script_dir}/env/scripts/util.sh

cd "$script_dir"

cd hmalloc
make -j$(corecount) || exit 1
cp lib/libhmalloc.so ${script_dir}/env/lib
cp src/proc_object_map.h ${script_dir}/env/include
