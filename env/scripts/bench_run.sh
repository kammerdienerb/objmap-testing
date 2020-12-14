#!/usr/bin/env bash

this_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source "${this_dir}/check_env.sh"
source "${this_dir}/util.sh"

if [ -z "${BENCH_DIR}" ]; then
    hm_err "bench_run.sh" "BENCH_DIR is not set."
fi

if ! [ -f "${BENCH_DIR}/bench_run.sh" ]; then
    hm_err "bench_run.sh" "${BENCH_DIR}/bench_run.sh not found."
fi

if [ -z "${CONFIG}" ]; then
    hm_err "bench_run.sh" "CONFIG is not set."
fi

if ! [ -f "${CONFIG}" ]; then
    hm_err "bench_run.sh" "${CONFIG} not found."
fi

source ${BENCH_DIR}/bench_run.sh

if ! [ "$(type -t do_bench_run 2>/dev/null)" = "function" ]; then
    hm_err "bench_run.sh" "Missing bash function 'do_bench_run' from '${BENCH_DIR}/bench_run.sh'."
fi

source ${CONFIG}

config_name=$(basename ${CONFIG} '.sh')

# check config for required fields
if [ -z "${EXE}" ]; then
    hm_err "bench_run.sh" "CONFIG missing \$EXE."
fi
if [ -z "${INPUT}" ]; then
    hm_err "bench_run.sh" "CONFIG missing \$INPUT."
fi


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

if ! [ -z "${TOOL}" ]; then
    TOOL_REL=$(echo "${TOOL}" | awk '{ print $1; }')
    TOOL_ABS=$(realpath "${TOOL_REL}")
    TOOL=${TOOL/${TOOL_REL}/${TOOL_ABS}}
fi

cd ${BENCH_DIR}

ITER=i"${ITER:-0}"

mkdir -p results/${config_name}/${ITER}

export output_file="$(realpath results/${config_name}/${ITER}/stdout)"

function run_prepared_cmd {
    script -q -c "/usr/bin/time -v ${CMD}" /dev/null 2>&1 | tee ${output_file}

    if [ -f hmalloc.log ]; then
        mv hmalloc.log results/${config_name}/${ITER}
    fi
}

echo ""
echo "COMMAND: $CMD"
echo ""

if ! [ -z "${TOOL}" ]; then
    if [ ${RUN_TOOL_AS_ROOT} = "yes" ]; then
        echo "TOOL: sudo ${TOOL}"
        echo ""
    else
        echo "TOOL: ${TOOL}"
        echo ""
    fi
fi

echo "###################### BEGIN COMMAND OUTPUT ######################"
do_bench_run&

# Find the PID of the experiment we just launched.
while true; do
    CMD_PID=$(ps ax -o pid,cmd |
              awk '{ for (i = 2; i <= NF; i++) { printf("%s ", $i); } printf("%s\n", $1); }' |
              grep ^$(realpath "${EXE}") |
              head -n 1 |
              awk '{ print $NF; }')

    if [ "${CMD_PID}x" != "x" ]; then
        break;
    fi
done

# If there's a tool, run it.
if ! [ -z "${TOOL}" ]; then
    TOOL_NAME=$(basename $(echo "${TOOL}" | awk '{ print $1; }'))
    TOOL_CMD=$(echo "${TOOL}" | sed "s/%p/${CMD_PID}/g")

    if [ ${RUN_TOOL_AS_ROOT} = "yes" ]; then
        sudo ${TOOL_CMD} > results/${config_name}/${ITER}/${TOOL_NAME}.out 2>&1 &
    else
        ${TOOL_CMD} > results/${config_name}/${ITER}/${TOOL_NAME}.out 2>&1 &
    fi

    TOOL_PID=$!
fi

function cleanup {
    echo ""
    echo "SIGINT received..."
    if ! [ -z "${TOOL}" ]; then
        echo "    Killing tool: (kill ${TOOL_PID})"
        if [ ${RUN_TOOL_AS_ROOT} = "yes" ]; then
            sudo kill ${TOOL_PID}
        else
            kill ${TOOL_PID}
        fi
    fi
    echo "    Killing experiment: (kill ${CMD_PID})"
    kill ${CMD_PID}
}

trap "cleanup; exit 1" INT
wait
