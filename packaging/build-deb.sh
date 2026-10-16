#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-"$source_dir/build-deb"}
cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_GUI=ON -DBUILD_TOOLS=OFF -DBUILD_TESTING=OFF
cmake --build "$build_dir" -j4
(cd "$build_dir" && cpack -G DEB)

python3 "$source_dir/packaging/make-source.py" "$build_dir"
