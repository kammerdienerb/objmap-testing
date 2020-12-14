#!/usr/bin/env bash

this_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source "${this_dir}/check_env.sh"
source "${this_dir}/util.sh"

if [ -z "${BENCH_DIR}" ]; then
    hm_err "bench_debug.sh" "BENCH_DIR is not set."
fi

if ! [ -f "${BENCH_DIR}/bench_run.sh" ]; then
    hm_err "bench_debug.sh" "${BENCH_DIR}/bench_run.sh not found."
fi

if [ -z "${CONFIG}" ]; then
    hm_err "bench_debug.sh" "CONFIG is not set."
fi

if ! [ -f "${CONFIG}" ]; then
    hm_err "bench_debug.sh" "${CONFIG} not found."
fi

source ${BENCH_DIR}/bench_run.sh

if ! [ "$(type -t do_bench_run 2>/dev/null)" = "function" ]; then
    hm_err "bench_debug.sh" "Missing bash function 'do_bench_run' from '${BENCH_DIR}/bench_run.sh'."
fi

source ${CONFIG}

config_name=$(basename ${CONFIG} '.sh')

# check config for required fields
if [ -z "${EXE}" ]; then
    hm_err "bench_debug.sh" "CONFIG missing \$EXE."
fi
if [ -z "${INPUT}" ]; then
    hm_err "bench_debug.sh" "CONFIG missing \$INPUT."
fi


# Do it.
options=""
if ! [ -z "${HMALLOC_SITE_LAYOUT}" ]; then
    options+=" HMALLOC_SITE_LAYOUT=${HMALLOC_SITE_LAYOUT}"
fi
if ! [ -z "${HMALLOC_OBJMAP_MODE}" ]; then
    options+=" HMALLOC_OBJMAP_MODE=${HMALLOC_OBJMAP_MODE}"
fi

with_env="env ${ENV} ${options} $(realpath ${BENCH_DIR}/${EXE})"

input_name="input_$INPUT"
export CMD="${with_env} ${!input_name}"

cd ${BENCH_DIR}

function run_prepared_cmd {
    gdb -ex 'set follow-fork-mode child' --args ${CMD}
}

echo $CMD
do_bench_run
