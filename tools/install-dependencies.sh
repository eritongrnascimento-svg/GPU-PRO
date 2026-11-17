#!/bin/sh
# No shell evaluation of os-release and no vendor drivers/repositories installed.
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
LC_ALL=C
export LC_ALL
mode=${1:---plan}
root=/
if [ "${2:-}" = --root ] && [ "$mode" = --plan ]; then root=${3:?}; fi
os="$root/etc/os-release"
read_key() { sed -n "s/^$1=//p" "$os" 2>/dev/null | head -n1 | tr -d '\042\047' | tr -cd 'a-zA-Z0-9._ -'; }
id=$(read_key ID); like=$(read_key ID_LIKE)
family=''
case " $id $like " in *' nixos '*|*' silverblue '*|*' kinoite '*|*' bazzite '*|*' steamos '*) family=immutable;; esac
if [ -e "$root/run/ostree-booted" ] || [ -d "$root/nix/store" ]; then family=immutable; fi
if [ -z "$family" ]; then
 case " $id $like " in
  *' ubuntu '*|*' debian '*|*' linuxmint '*|*' anduinos '*) family=apt;;
  *' arch '*|*' cachyos '*|*' manjaro '*|*' endeavouros '*) family=pacman;;
  *' fedora '*|*' rhel '*|*' centos '*) family=dnf;;
  *' opensuse '*|*' opensuse-tumbleweed '*|*' opensuse-leap '*|*' suse '*) family=zypper;;
 esac
fi
fail() { printf '{"ok":false,"system":"%s","error":"%s"}\n' "$id" "$1"; exit 2; }
[ "$mode" = --plan ] || [ "$mode" = --apply ] || fail 'Opção inválida'
[ "$family" != immutable ] || fail 'Sistema declarativo ou imutável: use o método nativo do sistema; nenhuma instalação imperativa foi preparada'
choose_apt() {
 for candidate in "$@"; do
  if [ "$root" != / ] || apt-cache policy "$candidate" 2>/dev/null | sed -n '/Candidate:/p' | grep -qv '(none)'; then printf '%s' "$candidate"; return 0; fi
 done
 return 1
}
case "$family" in
 apt)
  set -- /usr/bin/apt-get install
  for component in core dbus gui network opengl widgets; do
   if [ "$component" = opengl ]; then stem=libqt6opengl6; else stem=libqt6${component}6; fi
   pkg=$(choose_apt "${stem}t64" "$stem") || fail 'Qt 6 exigido não está disponível nos repositórios configurados'
   set -- "$@" "$pkg"
  done
  gtk=$(choose_apt libgtk-3-0t64 libgtk-3-0) || fail 'GTK 3 indisponível no repositório'
  archive=$(choose_apt libarchive13t64 libarchive13) || fail 'libarchive indisponível no repositório'
  set -- "$@" "$gtk" "$archive" libdrm2 libsqlite3-0 libvulkan1 libx11-6 qt6-qpa-plugins qt6-wayland xwayland pkexec xterm ffmpeg
  # Optional integrations are included only when already available in configured repos.
  for optional in mangohud gamescope vkbasalt vulkan-tools mesa-utils vainfo lm-sensors; do
   if [ "$root" = / ] && choose_apt "$optional" >/dev/null; then set -- "$@" "$optional"; fi
  done
  note='Dependências do sistema; nomes Qt t64 são resolvidos pelo repositório atual. Isso não converte ABI nem torna um DEB de outra base compatível.'
  ;;
 pacman)
  set -- /usr/bin/pacman -Syu --needed qt6-base qt6-wayland gtk3 libdrm sqlite libarchive vulkan-icd-loader libx11 xorg-xwayland polkit xterm ffmpeg mangohud gamescope vkbasalt vulkan-tools mesa-utils libva-utils lm_sensors
  note='Arch e derivados exigem atualização completa pelo pacman para evitar atualização parcial. Revise a transação no terminal.'
  ;;
 dnf)
  set -- /usr/bin/dnf install qt6-qtbase qt6-qtbase-gui qt6-qtwayland gtk3 libdrm sqlite-libs libarchive vulkan-loader libX11 xorg-x11-server-Xwayland polkit xterm ffmpeg-free mangohud vulkan-tools lm_sensors
  note='Usa somente os repositórios existentes. FFmpeg da base pode não incluir todos os codecs; nenhum repositório externo será adicionado.'
  ;;
 zypper)
  set -- /usr/bin/zypper install libQt6Core6 libQt6DBus6 libQt6Gui6 libQt6Network6 libQt6OpenGL6 libQt6Widgets6 qt6-wayland libgtk-3-0 libdrm2 libsqlite3-0 libarchive13 libvulkan1 libX11-6 xwayland polkit xterm ffmpeg
  note='Usa somente os repositórios configurados. Disponibilidade de codecs varia conforme a base.'
  ;;
 *) fail 'Distribuição sem instalador reconhecido. Nenhum comando de outra distribuição será executado';;
esac
if [ "$mode" = --plan ]; then
 printf '{"ok":true,"system":"%s","manager":"%s","note":"%s","command":[' "$id" "$family" "$note"
 separator=''; for arg in "$@"; do printf '%s"%s"' "$separator" "$arg"; separator=,; done
 printf ']}\n'; exit 0
fi
printf '%s\n%s\n' "$note" 'A instalação não fornece sensores ausentes no driver e não corrige uma ABI incompatível.'
printf 'Executar instalação acima? [s/N] '
read -r answer
case "$answer" in s|S) ;; *) exit 0;; esac
if [ "$(id -u)" = 0 ]; then exec "$@"; fi
if command -v pkexec >/dev/null 2>&1; then exec pkexec "$@"; fi
exec sudo -- "$@"
