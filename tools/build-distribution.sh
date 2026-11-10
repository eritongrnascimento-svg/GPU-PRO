#!/bin/bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=$(realpath "${1:-build}")
output_dir=$(realpath -m "${2:-dist}")
mkdir -p "$output_dir"
version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$project_dir/release.json")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
sh "$project_dir/tools/package.sh" "$build_dir" "$output_dir"
# Debian package uses system libraries; the helper remains a separately authenticated executable.
debroot="$stage/deb"
mkdir -p "$debroot/usr/lib/gpu-pro/bin" "$debroot/usr/libexec/gpu-pro" "$debroot/usr/bin" "$debroot/usr/share/applications" "$debroot/usr/share/icons/hicolor/512x512/apps" "$debroot/usr/share/doc/gpu-pro" "$debroot/DEBIAN"
for binary in linux-gpu-control gpu-bench-worker gpu-video-worker gpu-diagnostics; do install -m755 "$build_dir/$binary" "$debroot/usr/lib/gpu-pro/bin/"; done
install -m755 "$build_dir/gpu-clock-helper" "$debroot/usr/libexec/gpu-pro/"
printf '#!/bin/sh\nexec /usr/lib/gpu-pro/bin/linux-gpu-control "$@"\n' > "$debroot/usr/bin/gpu-pro"
chmod 755 "$debroot/usr/bin/gpu-pro"
cp "$project_dir/packaging/gpu-pro.desktop" "$debroot/usr/share/applications/"
cp "$project_dir/assets/gpu.png" "$debroot/usr/share/icons/hicolor/512x512/apps/gpu-pro.png"
cp -R "$project_dir/docs" "$project_dir/LICENSE" "$project_dir/NOTICE" "$project_dir/README.md" "$debroot/usr/share/doc/gpu-pro/"
mkdir -p "$stage/shlibs/debian"; printf 'Source: gpu-pro\nSection: utils\nPriority: optional\nMaintainer: Ériton do Nascimento <eritongrnascimento-svg@users.noreply.github.com>\nStandards-Version: 4.6.2\n\nPackage: gpu-pro\nArchitecture: amd64\nDescription: Linux MultiGPU monitor and benchmark panel\n' > "$stage/shlibs/debian/control"
exe_args=(); for binary in "$debroot"/usr/lib/gpu-pro/bin/* "$debroot"/usr/libexec/gpu-pro/*; do exe_args+=("-e$binary"); done
depends=$(cd "$stage/shlibs" && dpkg-shlibdeps -O "${exe_args[@]}" | sed -n 's/^shlibs:Depends=//p')
cat > "$debroot/DEBIAN/control" <<CONTROL
Package: gpu-pro
Version: $version
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Ériton do Nascimento <eritongrnascimento-svg@users.noreply.github.com>
Depends: $depends
Recommends: ffmpeg, policykit-1, x-terminal-emulator
Homepage: https://github.com/eritongrnascimento-svg/GPU-PRO
Description: Monitoramento e benchmarks MultiGPU Linux
 Interface Qt para NVIDIA, AMD e Intel, overlay e ferramentas de benchmark.
CONTROL
dpkg-deb --root-owner-group --build "$debroot" "$output_dir/gpu-pro_${version}_ubuntu24.04_amd64.deb"
dpkg-deb --info "$output_dir/gpu-pro_${version}_ubuntu24.04_amd64.deb"
# A single executable launcher containing all workers, using host libraries.
mkdir -p "$stage/portable"; tar -xzf "$output_dir/GPU-PRO-$version-linux-x86_64.tar.gz" -C "$stage/portable"
makeself --gzip --sha256 --nox11 --notemp "$stage/portable/GPU-PRO" "$output_dir/GPU-PRO-$version-linux-x86_64.run" "GPU-PRO $version" ./run.sh
sh "$output_dir/GPU-PRO-$version-linux-x86_64.run" --check
# Fetch official AppImage build tools and record their identity.
python3 "$project_dir/tools/appimage-tools.py" "$stage/tools"
appdir="$stage/GPU-PRO.AppDir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications" "$appdir/usr/share/icons/hicolor/512x512/apps" "$appdir/usr/share/doc/gpu-pro"
for binary in linux-gpu-control gpu-bench-worker gpu-video-worker gpu-diagnostics gpu-clock-helper; do install -m755 "$build_dir/$binary" "$appdir/usr/bin/"; done
cp "$project_dir/packaging/gpu-pro.desktop" "$appdir/usr/share/applications/"
sed -i 's/^Exec=gpu-pro$/Exec=linux-gpu-control/' "$appdir/usr/share/applications/gpu-pro.desktop"
cp "$project_dir/assets/gpu.png" "$appdir/usr/share/icons/hicolor/512x512/apps/gpu-pro.png"
cp -R "$project_dir/docs" "$project_dir/LICENSE" "$project_dir/NOTICE" "$project_dir/README.md" "$appdir/usr/share/doc/gpu-pro/"
export APPIMAGE_EXTRACT_AND_RUN=1 QMAKE=/usr/bin/qmake6
export PATH="$stage/tools:$PATH"
args=(); for binary in "$appdir"/usr/bin/*; do args+=(--executable "$binary"); done
"$stage/tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" "${args[@]}" --desktop-file "$appdir/usr/share/applications/gpu-pro.desktop" --icon-file "$appdir/usr/share/icons/hicolor/512x512/apps/gpu-pro.png" --exclude-library='libstdc++.so.6' --exclude-library='libgcc_s.so.1' --plugin qt
# Include notices and retrieve exact corresponding source packages for bundled libraries.
python3 "$project_dir/tools/dependency-sources.py" "$appdir" "$output_dir" "$version"
export ARCH=x86_64 OUTPUT="$output_dir/GPU-PRO-$version-x86_64.AppImage"
"$stage/tools/appimagetool-x86_64.AppImage" "$appdir"
chmod +x "$OUTPUT"
QT_QPA_PLATFORM=offscreen "$OUTPUT" --appimage-extract-and-run --ui-check
(cd "$output_dir" && sha256sum "GPU-PRO-$version-linux-x86_64.tar.gz" "GPU-PRO-$version-source.tar.gz" "GPU-PRO-$version-linux-x86_64.run" "GPU-PRO-$version-x86_64.AppImage" "gpu-pro_${version}_ubuntu24.04_amd64.deb" "GPU-PRO-$version-dependency-sources.tar.gz" > SHA256SUMS.txt)
