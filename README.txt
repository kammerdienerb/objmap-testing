################################### objmap-testing ###################################

*** This requires the modified version of the linux kernel version 5.7.2

1. Setup environment variables using provided scripts:
    For CentOS: env/setup_env_centos.sh
    Other:      env/setup_env.sh

    ${OBJMAP_ENV} should be set to "yes".

2. Run build.sh
    Most dependencies will be fetched and built for you, however
    a CMake >= 3.4.3, gcc >= 7.2.0, and libpfm must be available
    on the system.
    These requirements will be checked and error messages will tell
    you if you are missing any.

3. Build tools:
    cd examples && ./build_all.sh

4. Get benchmarks:
    git clone https://github.com/kammerdienerb/objmap-benchmarks benchmarks

    More benchmarks can be added as long as they follow the layout of the
    provided benchmarks:
        - bench_{build,run}.sh in the top-level directory of the benchmark.
        - a modified build system (e.g. Makefile) that will use the compiler
          wrapper scripts
        (see the provided lulesh directory for examples of these)

5. Build a benchmark:
    Run bench_build.sh (should be in PATH if you've run setup_env.sh) with
    ${BENCH_DIR} set to the path of the benchmark.

    Ex. BENCH_DIR=benchmarks/lulesh bench_build.sh

6. Run the benchmark with a config:
    Similar to building a benchmark, but takes more options.
    See example_of_how_to_run.sh.

7. Collect results:
    Results will be found in ${BENCH_DIR}/results/${CONFIG}/${ITER}
