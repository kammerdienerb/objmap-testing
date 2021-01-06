#!/usr/bin/env bash

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd "$script_dir"

C_FLAGS="-Wall -Werror -O0 -g -I${OBJMAP_ENV_DIR}/include"
# C_FLAGS="-Wall -Werror -O3 -I${OBJMAP_ENV_DIR}/include"
LD_FLAGS="-L${OBJMAP_ENV_DIR}/lib -lm -lpfm -lpthread"

pids=()

mkdir -p obj
rm -f obj/*
for f in $(find src -name "*.c"); do
    echo "CC   ${f}"
    gcc -c ${C_FLAGS} -o obj/$(basename ${f} ".c").o ${f} &
    pids+=($!)
done

for p in ${pids[@]}; do
    wait $p || exit 1
done

mkdir -p bin
rm -f bin/*
echo "LD   mat-daemon"
gcc ${LD_FLAGS} -o bin/mat-daemon obj/*.o
echo "Done."
