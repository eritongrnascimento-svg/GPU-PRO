#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -x "$project_dir/build/linux-gpu-control" ]; then
 executable="$project_dir/build/linux-gpu-control"
else
 executable="$project_dir/bin/linux-gpu-control"
fi
if [ ! -x "$executable" ]; then
 printf 'Executável não encontrado. Compile com %s/build.sh\n' "$project_dir" >&2
 exit 127
fi
if command -v ldd >/dev/null 2>&1; then
 dependencies=$(ldd "$executable" 2>&1) || true
 case "$dependencies" in
  *"not found"*) printf 'Bibliotecas de execução ausentes:\n%s\nRecompile nesta distribuição com build.sh após instalar as dependências do README.\n' "$dependencies" >&2; exit 127 ;;
 esac
fi
exec "$executable" "$@"
