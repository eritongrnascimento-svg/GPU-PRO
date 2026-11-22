#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-"$project_dir/build"}
output_dir=${2:-"$project_dir/dist"}
mkdir -p "$output_dir"
output_dir=$(CDPATH= cd -- "$output_dir" && pwd)
version=$(sed -n 's/.*"version": "\([0-9.]*\)".*/\1/p' "$project_dir/release.json")
[ -n "$version" ] || exit 2
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM
mkdir -p "$stage/GPU-PRO/bin"
for file in README.md LICENSE NOTICE TRADEMARKS.md CONTRIBUTING.md CHANGELOG.md SECURITY.md CMakeLists.txt .gitignore release.json run.sh build.sh; do
 cp "$project_dir/$file" "$stage/GPU-PRO/"
done
for directory in src core windows shaders assets docs examples tools packaging .github; do
 if [ "$directory" = windows ]; then mkdir -p "$stage/GPU-PRO/windows"; cp -R "$project_dir/windows/native" "$project_dir/windows/GpuPro.WinUI" "$project_dir/windows/checks" "$project_dir/windows/installer" "$project_dir/windows/build-windows.ps1" "$stage/GPU-PRO/windows/"; find "$stage/GPU-PRO/windows" -type d \( -name bin -o -name obj \) -prune -exec rm -rf {} +; else cp -R "$project_dir/$directory" "$stage/GPU-PRO/"; fi
done
# Source package never includes platform-specific binaries or local configuration.
tar -czf "$output_dir/GPU-PRO-$version-source.tar.gz" -C "$stage" GPU-PRO
for executable in linux-gpu-control gpu-bench-worker gpu-video-worker gpu-diagnostics gpu-clock-helper gpu-system-worker gpu-cpu-worker; do
 [ -x "$build_dir/$executable" ] || { printf 'Executável ausente: %s\n' "$executable" >&2; exit 2; }
 cp "$build_dir/$executable" "$stage/GPU-PRO/bin/"
done
for extra in libgpupro_fps.so gpu-pro-fps.json gpu-pro-fps; do
 [ -f "$build_dir/$extra" ] || { printf 'Arquivo ausente: %s
' "$extra" >&2; exit 2; }
 cp "$build_dir/$extra" "$stage/GPU-PRO/bin/"
done
chmod 755 "$stage/GPU-PRO/bin/gpu-pro-fps"
chmod 755 "$stage/GPU-PRO/run.sh" "$stage/GPU-PRO/build.sh" "$stage/GPU-PRO/tools/package.sh"
tar -czf "$output_dir/GPU-PRO-$version-linux-x86_64.tar.gz" -C "$stage" GPU-PRO
(cd "$output_dir" && sha256sum "GPU-PRO-$version-linux-x86_64.tar.gz" "GPU-PRO-$version-source.tar.gz" > SHA256SUMS.txt)
printf 'Pacotes criados em %s\n' "$output_dir"
