%global debug_package %{nil}
%global _build_id_links none

Name:           gpu-pro
Version:        %{gpu_pro_version}
Release:        1%{?dist}
Summary:        Monitoramento e benchmarks MultiGPU Linux
License:        GPL-3.0-only
URL:            https://github.com/eritongrnascimento-svg/GPU-PRO
Recommends:     /usr/bin/pkexec
ExclusiveArch:  x86_64

%description
Interface Qt para NVIDIA, AMD e Intel, overlay e ferramentas de benchmark.

%install
bin=%{buildroot}%{_prefix}/lib/gpu-pro/bin
install -d "$bin" "%{buildroot}%{_bindir}"
for item in linux-gpu-control gpu-bench-worker gpu-video-worker gpu-diagnostics gpu-system-worker gpu-cpu-worker gpu-pro-fps; do
    install -m755 "%{artifact_dir}/$item" "$bin/"
done
install -m644 "%{artifact_dir}/libgpupro_fps.so" "%{artifact_dir}/gpu-pro-fps.json" "$bin/"
install -Dm755 "%{artifact_dir}/gpu-clock-helper" "%{buildroot}%{_libexecdir}/gpu-pro/gpu-clock-helper"

for item in gpu-pro gpu-pro-fps; do
    target=$item
    if [ "$item" = gpu-pro ]; then target=linux-gpu-control; fi
    printf '#!/bin/sh\nexec %%s "$@"\n' "%{_prefix}/lib/gpu-pro/bin/$target" > "%{buildroot}%{_bindir}/$item"
    chmod 755 "%{buildroot}%{_bindir}/$item"
done

install -Dm644 "%{project_dir}/packaging/gpu-pro.desktop" "%{buildroot}%{_datadir}/applications/gpu-pro.desktop"
install -Dm644 "%{project_dir}/packaging/gpu-pro.svg" "%{buildroot}%{_datadir}/icons/hicolor/scalable/apps/gpu-pro-transparent-v2.svg"
install -Dm644 "%{project_dir}/assets/gpu-pro.png" "%{buildroot}%{_datadir}/pixmaps/gpu-pro.png"
install -Dm644 "%{project_dir}/packaging/io.github.eritongrnascimento_svg.GPU-PRO.metainfo.xml" "%{buildroot}%{_metainfodir}/io.github.eritongrnascimento_svg.GPU-PRO.metainfo.xml"
install -Dm644 "%{project_dir}/LICENSE" "%{buildroot}%{_licensedir}/gpu-pro/LICENSE"
install -Dm644 "%{project_dir}/TRADEMARKS.md" "%{buildroot}%{_licensedir}/gpu-pro/TRADEMARKS.md"
install -Dm644 "%{project_dir}/NOTICE" "%{buildroot}%{_licensedir}/gpu-pro/NOTICE"
install -Dm644 "%{project_dir}/README.md" "%{buildroot}%{_docdir}/gpu-pro/README.md"
cp -R "%{project_dir}/docs" "%{buildroot}%{_docdir}/gpu-pro/"

%files
%{_bindir}/gpu-pro
%{_bindir}/gpu-pro-fps
%{_prefix}/lib/gpu-pro/
%{_libexecdir}/gpu-pro/
%{_datadir}/applications/gpu-pro.desktop
%{_datadir}/icons/hicolor/scalable/apps/gpu-pro-transparent-v2.svg
%{_datadir}/pixmaps/gpu-pro.png
%{_metainfodir}/io.github.eritongrnascimento_svg.GPU-PRO.metainfo.xml
%doc %{_docdir}/gpu-pro/
%license %{_licensedir}/gpu-pro/
