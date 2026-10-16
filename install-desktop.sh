#!/bin/sh
# Install the launch entry for the current desktop user, after cmake --install.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
prefix=${1:-"$HOME/.local"}
test -x "$prefix/bin/itl612-cm5"
mkdir -p "$prefix/bin" "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/1024x1024/apps"
install -m 0755 "$source_dir/itl612-start" "$prefix/bin/itl612-start"
install -m 0644 "$source_dir/assets/itl612-cm5.png" "$HOME/.local/share/icons/hicolor/1024x1024/apps/itl612-cm5.png"
entry="$HOME/.local/share/applications/itl612-cm5.desktop"
# Desktop Exec quoting also works with spaces in the install prefix.
escaped_exec=$(printf '%s' "$prefix/bin/itl612-start" | sed 's/[\\"`$]/\\&/g')
sed '/^Exec=/d' "$source_dir/itl612-cm5.desktop" > "$entry"
printf 'Exec="%s"\n' "$escaped_exec" >> "$entry"
desktop-file-validate "$entry"
desktop_dir=$(xdg-user-dir DESKTOP)
mkdir -p "$desktop_dir"
install -m 0755 "$entry" "$desktop_dir/itl612-cm5.desktop"
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$HOME/.local/share/applications"
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "$HOME/.local/share/icons/hicolor" >/dev/null 2>&1 || true
fi
printf 'Desktop launcher installed: %s\n' "$desktop_dir/itl612-cm5.desktop"
