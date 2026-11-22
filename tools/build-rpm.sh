#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=$(realpath "${1:-build}")
output_dir=$(realpath -m "${2:-dist}")
mkdir -p "$output_dir"
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
version=$(sed -n 's/.*"version": "\([0-9.]*\)".*/\1/p' "$project_dir/release.json")
test -n "$version"
rpmbuild -bb "$project_dir/packaging/gpu-pro.spec" \
  --define "_topdir $stage" --define "_tmppath $stage" --define "_rpmdir $output_dir" \
  --define "_build_name_fmt %{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}.rpm" \
  --define "gpu_pro_version $version" --define "project_dir $project_dir" \
  --define "artifact_dir $build_dir"
