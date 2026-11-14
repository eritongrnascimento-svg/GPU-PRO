# Instalar e executar o GPU-PRO

Leia primeiro o [aviso de riscos e responsabilidade](AVISO-DE-RESPONSABILIDADE.md). O aceite na aba Overclock não aplica ajustes automaticamente.

Repositório oficial: https://github.com/eritongrnascimento-svg/GPU-PRO

Linux x86_64, driver do fabricante, Vulkan e/ou OpenGL funcionais. A interface não exige root. O helper é opcional para ajustes de clocks; benchmarks, sensores e feedback não precisam dele.

## Compilar pelo GitHub

```sh
git clone https://github.com/eritongrnascimento-svg/GPU-PRO.git
cd GPU-PRO
sh build.sh
sh run.sh
```

`build.sh` verifica dependências e não instala pacotes automaticamente. O build em `build/` tem preferência sobre `bin/` no launcher.

### Arch / CachyOS

```sh
sudo pacman -S --needed base-devel cmake pkgconf qt6-base gtk3 libx11 libdrm vulkan-headers vulkan-icd-loader glslang sqlite libarchive
```

### Debian / Ubuntu com Qt 6.4 ou posterior

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config qt6-base-dev libqt6opengl6-dev libgtk-3-dev libx11-dev libvulkan-dev libdrm-dev glslang-tools libsqlite3-dev libarchive-dev
```

Os workflows públicos do [GitHub Actions](https://github.com/eritongrnascimento-svg/GPU-PRO/actions) registram compilação e verificações no Ubuntu 24.04. A validação física registrada foi realizada no CachyOS; consulte [VERIFICACAO.md](VERIFICACAO.md) para distinguir testes automatizados de testes em hardware.

O driver/ICD da GPU deve ser instalado pelos métodos da distribuição/fabricante. Não copie bibliotecas NVIDIA para computadores AMD/Intel. Em computadores híbridos, selecione a GPU no painel e confira a verificação do dispositivo no resultado do teste.

### Vídeo e integrações opcionais

Instale FFmpeg da distribuição com suporte CUDA/NVENC ou VA-API correspondente à GPU. Para Intel/AMD, o driver VA-API adequado também deve estar instalado. A suíte mostra codecs incompatíveis e falhas reais; não utiliza encoder/decoder CPU para fabricar resultado de hardware.

MangoHud, vkBasalt e Gamescope só são necessários para os perfis que os utilizam. FSR/DLSS/geração de quadros dependem dos jogos e das respectivas integrações; o painel não injeta essas tecnologias universalmente.

`gpustat`, `nvidia-ml-py` e `psutil` são ferramentas opcionais de diagnóstico Python. Não são dependências do aplicativo C++. Instalar bindings NVML não cria sensores que o driver não expõe. Respeite o gerenciamento de Python da distribuição.

## Pacote binário portátil

Na [última release](https://github.com/eritongrnascimento-svg/GPU-PRO/releases/latest), obtenha `GPU-PRO-<versão>-linux-x86_64.tar.gz` e `SHA256SUMS.txt`. Na mesma pasta:

```sh
sha256sum --check --ignore-missing SHA256SUMS.txt
tar -xzf GPU-PRO-0.7.0-linux-x86_64.tar.gz
cd GPU-PRO
sh run.sh
```

Os pacotes oficiais atuais são produzidos no Ubuntu 24.04; a versão 0.3 teve binários locais gerados no CachyOS. TAR/RUN dependem das bibliotecas do sistema e não são AppImages universais. Se houver incompatibilidade de Qt/glibc ou biblioteca ausente, compile pelo código-fonte. Extraia o pacote portátil em uma pasta gravável pelo seu usuário para usar atualizações com backup pelo painel.

## Helper opcional de clocks

Não execute a interface com sudo. Revise o código e instale somente o helper:

```sh
sudo install -d -o root -g root -m 0755 /usr/local/libexec/linux-gpu-control
sudo install -o root -g root -m 0755 build/gpu-clock-helper /usr/local/libexec/linux-gpu-control/gpu-clock-helper
```

No pacote binário, substitua `build/gpu-clock-helper` por `bin/gpu-clock-helper`. É necessário `pkexec` e um agente de autenticação do ambiente gráfico. Não há setuid, serviço ou execução automática. O helper não aceita comandos de tensão nem de power limit.

## Primeira execução

1. Confira identificação e sensores em **GPU → Monitor de recursos / Especificações**.
2. Em **GamerMonitor**, configure formato/posição do HUD. A opção Superior centralizado está disponível. Role a página para acessar Métrica do dispositivo.
3. Em **Benchmark**, use as abas 3D/Jogos, Computação, Encoder/Decoder e Gargalo. Comece com a configuração original e 720p/30 s.
4. Consulte o histórico e salve PNG/JPEG/JSON. Gere feedback apenas depois de revisar o conteúdo.
5. Clocks são opcionais e não são aplicados ao abrir o painel. Teste curto não equivale a estabilidade prolongada.

Dados são preservados em `~/.config/LinuxGPUControl/LinuxGPUControl/` (ou equivalente XDG). O nome interno foi mantido para reutilizar os históricos existentes. Excluir o aplicativo não exclui esses dados automaticamente.

## Atualizar e restaurar

**Atualizações → Verificar atualizações** consulta releases estáveis do repositório oficial. Se você já estiver na versão estável mais recente, o painel informa que não há atualização disponível.

Ao atualizar uma instalação portátil TAR/RUN pelo painel, o pacote é verificado e preparado, um backup é salvo em `updates/backups/` no diretório de configuração, e os arquivos são substituídos. Reinicie o aplicativo. O helper instalado em `/usr/local/libexec` não é alterado por esse fluxo.

Use **Restaurar último backup** se necessário. Caso a interface não abra após uma atualização, localize o backup indicado em `updates/last-backup.txt` e copie o conteúdo de `files/` de volta para a pasta original do aplicativo. `backup.json` informa a instalação correspondente e os arquivos introduzidos pela atualização. Não restaure dados de outra instalação.

Instalações compiladas em `build/`: atualize o checkout e rode `sh build.sh`. Pacotes de uma distribuição devem usar o gerenciador dessa distribuição.

## Verificações sem GPU

```sh
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/linux-gpu-control --ui-check
```

Consulte [VALIDACAO.md](VALIDACAO.md) para participar da validação em outras GPUs e [VERIFICACAO.md](VERIFICACAO.md) para consultar os testes registrados por versão.


## Formatos de instalação

- Ubuntu 24.04 amd64: `sudo apt install ./gpu-pro_0.7.0_ubuntu24.04_amd64.deb`; iniciar com `gpu-pro` ou pelo menu. O helper instalado em `/usr/libexec/gpu-pro/` continua exigindo autenticação para cada ajuste.
- AppImage x86_64: `chmod +x GPU-PRO-0.7.0-x86_64.AppImage` e executar o arquivo. Precisa de glibc 2.39 ou posterior e driver instalado; se FUSE não estiver disponível, use `--appimage-extract-and-run`. Bibliotecas/avisos podem ser inspecionados com `--appimage-extract`.
- Executável portátil: `chmod +x GPU-PRO-0.7.0-linux-x86_64.run` e executar. Extrai a pasta GPU-PRO com todos os workers e abre o painel; não exige administrador. Usa bibliotecas do sistema, como o tar.gz.

Em Atualizações, consulte drivers, escolha pacote/versão e simule. A instalação abre o gerenciador nativo em terminal, solicita autenticação do sistema e exige confirmação da transação. No Arch/CachyOS também atualiza os demais pacotes para evitar atualização parcial. O GPU-PRO não garante rollback de drivers; reinicie quando o gerenciador indicar.


A simulação de drivers Arch/CachyOS requer `bubblewrap` e suporte a namespaces de usuário. APT/DNF usam seus mecanismos nativos de prévia. Consulte [Drivers e sistema](DRIVERS-E-SISTEMA.md) para atualização DEB/AppImage, restauração e preferências de GPU/áudio.

## Ícones e painel já aberto

A partir da 0.7.0, abrir novamente pelo menu solicita a janela existente. Ao migrar de uma versão anterior, encerre as cópias antigas uma vez antes de abrir a versão atual. Consulte [Ícones, bandeja e instância única](IDENTIDADE-E-INSTANCIA.md) para caches, atalhos fixados e controle da animação.
