#!/usr/bin/env bash

build_llvm="yes"
build_flang_driver="yes"
build_libpgmath="yes"
build_openmp="yes"
build_flang="yes"
build_hmalloc="yes"
build_compass="yes"

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

source ${script_dir}/env/scripts/util.sh

cd "$script_dir"
mkdir -p build
install_dir=$(abspath ./env)

ncores=$(corecount)

### Check versions. ###
function version_gt() { test "$(printf '%s\n' "$@" | sort -V | head -n 1)" != "$1"; }

# CMake 3.4.3
min_cmake="3.4.3"
if [ "$(which cmake)" != "" ]; then
    cmake_ver=$(cmake --version | awk '{ if (NR==1) print $3 }')
    if ! version_gt "$cmake_ver" "$min_cmake"; then
        echo "CMake version ${min_cmake} or greater is required.."
        echo "    found version ${cmake_ver}."
        hm_err "build.sh" "Dependency error."
    fi
else
    echo "CMake version ${min_cmake} or greater is required.."
    echo "    did not find 'cmake'."
    hm_err "build.sh" "Dependency error."
fi

# GCC 7.2.0
min_gcc="7.2.0"
if [ "$(which gcc)" != "" ]; then
    gcc_ver=$(gcc -dumpfullversion -dumpversion)
    if ! version_gt "$gcc_ver" "$min_gcc"; then
        echo "gcc version ${min_gcc} or greater is required.."
        echo "    found version ${gcc_ver}."
        hm_err "build.sh" "Dependency error."
    fi
else
    echo "gcc version ${min_gcc} or greater is required.."
    echo "    did not find 'gcc'."
    hm_err "build.sh" "Dependency error."
fi

AWK_VERSION=$(awk -W version 2>&1 | head -n 1 | awk '{ print $1; }')
if ! grep "GNU" <(echo "$AWK_VERSION") > /dev/null 2>&1; then
    echo "awk needs to be gawk.."
    echo "    ${AWK_VERSION} is currently installed."
    hm_err "build.sh" "Dependency error."
fi

### Check for libpfm. ###
pfm_test_prg="#include <perfmon/pfmlib_perf_event.h>\nint main() { return 0; }"
if ! echo -e "${pfm_test_prg}" | gcc -x c -lpfm -o /dev/null -; then
    echo "libpfm test failed.."
    echo "    ensure that libpfm is available."
    hm_err "build.sh" "Dependency error."
fi

### Build LLVM ###
if ! [ -z "$build_llvm" ]; then
    if ! [ -d "llvm" ]; then
        git clone https://github.com/kammerdienerb/llvm
    else
        (cd "llvm"; git pull)
    fi

    (cd "llvm"; git checkout release_70)
    cd build
    rm -rf *

    CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=${install_dir}             \
                   -DCMAKE_BUILD_TYPE=RelWithDebInfo                 \
                   -DCMAKE_CXX_COMPILER=g++                          \
                   -DCMAKE_C_COMPILER=gcc                            \
                   -DLLVM_TARGETS_TO_BUILD=X86"

    cmake ${CMAKE_OPTIONS} ../llvm || exit 1
    make -j${ncores} || exit 1
    make install || exit 1

    cd ${script_dir}
fi

### Build the flang driver ###
if ! [ -z "$build_flang_driver" ]; then
    if ! [ -d "flang-driver" ]; then
        git clone https://github.com/kammerdienerb/flang-driver
    else
        (cd "flang-driver"; git pull)
    fi

    (cd "flang-driver"; git checkout release_70)
    cd build
    rm -rf *

    CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=${install_dir}             \
                   -DCMAKE_BUILD_TYPE=RelWithDebInfo                 \
                   -DCMAKE_CXX_COMPILER=g++                          \
                   -DCMAKE_C_COMPILER=gcc                            \
                   -DLLVM_CONFIG=${install_dir}/bin/llvm-config"

    cmake ${CMAKE_OPTIONS} ../flang-driver || exit 1
    make -j${ncores} || exit 1
    make install || exit 1

    cd ${script_dir}
fi

### Build OpenMP ###
if ! [ -z "$build_openmp" ]; then
    if ! [ -d "openmp" ]; then
        git clone https://github.com/kammerdienerb/openmp
    else
        (cd "openmp"; git pull)
    fi

    (cd "openmp"; git checkout release_70)
    cd build
    rm -rf *

    CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=${install_dir}             \
                   -DCMAKE_BUILD_TYPE=RelWithDebInfo                 \
                   -DCMAKE_CXX_COMPILER=${install_dir}/bin/clang++   \
                   -DCMAKE_C_COMPILER=${install_dir}/bin/clang       \
                   -DLLVM_CONFIG=${install_dir}/bin/llvm-config"

    cmake ${CMAKE_OPTIONS} ../openmp || exit 1
    make -j${ncores} || exit 1
    make install || exit 1

    cd ${script_dir}
fi

### Build libpgmath ###
if ! [ -z "$build_libpgmath" ]; then
    if ! [ -d "flang" ]; then
        git clone https://github.com/kammerdienerb/flang
    else
        (cd "flang"; git pull)
    fi

    (cd "flang"; git checkout release_70)
    cd build
    rm -rf *

    CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=${install_dir}             \
                   -DCMAKE_BUILD_TYPE=RelWithDebInfo                 \
                   -DCMAKE_CXX_COMPILER=${install_dir}/bin/clang++   \
                   -DCMAKE_C_COMPILER=${install_dir}/bin/clang       \
                   -DLLVM_CONFIG=${install_dir}/bin/llvm-config"

    cmake ${CMAKE_OPTIONS} ../flang/runtime/libpgmath || exit 1
    make -j${ncores} || exit 1
    make install || exit 1

    cd ${script_dir}
fi

### Build flang ###
if ! [ -z "$build_flang" ]; then
    if ! [ -d "flang" ]; then
        git clone https://github.com/kammerdienerb/flang
    else
        (cd "flang"; git pull)
    fi

    (cd "flang"; git checkout release_70)
    cd build
    rm -rf *

    CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=${install_dir}             \
                   -DCMAKE_BUILD_TYPE=RelWithDebInfo                 \
                   -DCMAKE_CXX_COMPILER=${install_dir}/bin/clang++   \
                   -DCMAKE_C_COMPILER=${install_dir}/bin/clang       \
                   -DLLVM_CONFIG=${install_dir}/bin/llvm-config"

    cmake ${CMAKE_OPTIONS} ../flang || exit 1
    make -j${ncores} || exit 1
    make install || exit 1

    cd ${script_dir}
fi

### Build hmalloc ###
if ! [ -z "$build_hmalloc" ]; then
    if ! [ -d "hmalloc" ]; then
        git clone https://github.com/kammerdienerb/hmalloc
    else
        (cd "hmalloc"; git pull)
    fi

    (cd "hmalloc"; git checkout objmap)

    cd hmalloc
    make clean
    make -j${ncores} || exit 1
    cp lib/libhmalloc.so ${install_dir}/lib
    cp src/proc_object_map.h ${install_dir}/include

    cd ${script_dir}
fi

### Build compass ###
if ! [ -z "$build_compass" ]; then
    if ! [ -d "compass" ]; then
        git clone https://github.com/kammerdienerb/compass
    else
        (cd "compass"; git pull)
    fi

    (cd "compass"; git checkout hmalloc)
    cd build
    rm -rf *

    CMAKE_OPTIONS="-DCMAKE_INSTALL_PREFIX=${install_dir}             \
                   -DCMAKE_BUILD_TYPE=RelWithDebInfo                 \
                   -DCMAKE_CXX_COMPILER=${install_dir}/bin/clang++   \
                   -DCMAKE_C_COMPILER=${install_dir}/bin/clang       \
                   -DLLVM_CONFIG=${install_dir}/bin/llvm-config"

    cmake ${CMAKE_OPTIONS} ../compass || exit 1
    make -j${ncores} || exit 1

    cp compass/libcompass.so ${install_dir}/lib

    cd ${script_dir}
fi

rm -rf build
