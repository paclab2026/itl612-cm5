#!/bin/sh
# Run as the desktop user after installing build dependencies.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${1:-"$source_dir/build"}
cmake -S "$source_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/.local" -DBUILD_TESTING=OFF -DBUILD_TOOLS=OFF
cmake --build "$build_dir" -j4
cmake --install "$build_dir"
sh "$source_dir/install-desktop.sh"
printf 'To grant camera access, run:\n'
printf '  sudo install -m 0644 "%s/70-itl612-mars.rules" /etc/udev/rules.d/\n' "$source_dir"
printf '  sudo udevadm control --reload-rules\nReconnect the camera, then launch ITL612 from the application menu.\n'
