#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
for tool in cmake c++ pkg-config glslangValidator; do
 if ! command -v "$tool" >/dev/null 2>&1; then
  printf 'Dependência de compilação ausente: %s\n' "$tool" >&2
  exit 2
 fi
done
for package in Qt6Widgets Qt6Network Qt6OpenGL Qt6DBus gtk+-3.0 pangocairo sqlite3 libarchive; do
 if ! pkg-config --exists "$package"; then
  printf 'Pacote de desenvolvimento ausente: %s\n' "$package" >&2
  exit 2
 fi
done
if [ -n "${VULKAN_HEADERS_DIR:-}" ]; then
 cmake -S "$project_dir" -B "$project_dir/build" -DCMAKE_BUILD_TYPE=Release "-DVulkan_INCLUDE_DIR=$VULKAN_HEADERS_DIR"
else
 cmake -S "$project_dir" -B "$project_dir/build" -DCMAKE_BUILD_TYPE=Release
fi
cmake --build "$project_dir/build" -j "${BUILD_JOBS:-4}"
