#!/usr/bin/env bash
# Linux build and offline tests only; never installs helpers or drivers.
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "$0")/.." && pwd)
shared="$source_dir/../../work/qa-20261113"
mkdir -p "$shared"
exec > >(tee -a "$shared/session.log") 2>&1
trap 'printf "stopped\n" > "$shared/session-state"' EXIT
printf 'starting\n' > "$shared/session-state"
cat /etc/os-release > "$shared/os-release.txt"
uname -a > "$shared/kernel.txt"
if ! command -v cmake >/dev/null || ! command -v glslangValidator >/dev/null || ! command -v rsync >/dev/null || ! pkg-config --exists Qt6Widgets Qt6Network Qt6OpenGL Qt6DBus gtk+-3.0 pangocairo sqlite3 libarchive libdrm; then
  sudo apt-get update
  sudo apt-get install -y build-essential cmake pkg-config qt6-base-dev libqt6opengl6-dev libgtk-3-dev libx11-dev libvulkan-dev libdrm-dev glslang-tools libsqlite3-dev libarchive-dev rsync
fi
build_root="$HOME/.cache/gpu-pro-validation-20261113"
mkdir -p "$build_root/source"
printf 'ready\n' > "$shared/session-state"
printf '1\n' > "$shared/request"
previous=''
deadline=$((SECONDS+3600))
while ((SECONDS<deadline)); do
  [[ ! -f "$shared/stop" ]] || break
  request=$(cat "$shared/request" 2>/dev/null || true)
  if [[ "$request" == "$previous" ]]; then sleep 2; continue; fi
  previous="$request"
  printf 'building %s\n' "$request" > "$shared/session-state"
  printf 'Compilando e verificando rodada %s\n' "$request"
  rsync -a --exclude=.git --exclude=build --exclude=bin --exclude=dist "$source_dir/" "$build_root/source/"
  set +e
  cmake -S "$build_root/source" -B "$build_root/build" -DCMAKE_BUILD_TYPE=Release > "$shared/configure.log" 2>&1
  result=$?
  if ((result==0)); then cmake --build "$build_root/build" -j 2 > "$shared/build.log" 2>&1; result=$?; fi
  if ((result==0)); then QT_QPA_PLATFORM=offscreen ctest --test-dir "$build_root/build" --output-on-failure > "$shared/ctest.log" 2>&1; result=$?; fi
  if ((result==0)); then
    export QT_QPA_PLATFORM=offscreen
    "$build_root/build/linux-gpu-control" --render-branding-preview "$shared/branding.png" > "$shared/preview.log" 2>&1
    for page in 2 4 6; do "$build_root/build/linux-gpu-control" --preview-offline --preview-page "$page" --screenshot "$shared/page-$page.png" --ui-smoke >> "$shared/preview.log" 2>&1 || result=$?; done
  fi
  set -e
  printf '{"request":"%s","exitCode":%d}\n' "$request" "$result" > "$shared/result.tmp"
  mv "$shared/result.tmp" "$shared/result.json"
  printf 'waiting %s (exit %s)\n' "$request" "$result" > "$shared/session-state"
  printf 'Rodada %s: exit %s. Aguardando correcoes; Ctrl+C encerra.\n' "$request" "$result"
done