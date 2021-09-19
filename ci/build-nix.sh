#! /bin/bash

set -e
set -x

# use RAM disk if possible
if [ "$CI" == "" ] && [ -d /dev/shm ]; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

if [ "$CI" == "" ]; then
    # reserve one core for the rest of the system on dev machines
    NPROC=$(nproc --ignore=1)
else
    # on the CI, we can just use everything we have
    NPROC=$(nproc)
fi

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" fragility-build-XXXXXX)

cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}

trap cleanup EXIT

# store repo root as variable
REPO_ROOT=$(readlink -f $(dirname $(dirname $0)))
OLD_CWD=$(readlink -f .)

pushd "$BUILD_DIR"

# can be overwritten by the user
BUILD_TYPE="${BUILD_TYPE:-Debug}"
cmake "$REPO_ROOT" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

make -j"$NPROC"
