# Arquitetura do GPU-PRO

```text
DRM / PCI / hwmon / NVML / fdinfo + /proc/stat (CPU)
              ↓
TelemetryService em QThread → sinais Qt → painel Widgets
              └── snapshots JSON / UDP 127.0.0.1
                         ↓
                processo GTK 3 / X11
                   Cairo + Pango

Painel → gpu-bench-worker (usuário) → Vulkan / OpenGL offscreen
       → FFmpeg assíncrono (usuário) → suíte de codecs
       → SQLite (local) → feedback JSON revisado, sem rede
       → pkexec → gpu-clock-helper (root, somente clocks)
                      └── watchdog independente / restauração no teste curto
```

## Overlay

O overlay adota a arquitetura utilizada no projeto LinTrucks VTC: processo separado, publicação independente da renderização da janela, snapshots completos em UDP loopback, GTK 3/GDK X11, RGBA, Cairo/Pango, sem decoração/foco/taskbar, região de entrada vazia, keep_above/override_redirect e elevação periódica a cada 2 s. O filho escolhe porta efêmera e sinaliza prontidão. Um token aleatório evita mistura casual entre instâncias; não é isolamento contra outro processo malicioso do mesmo usuário.

O publicador usa timer de 250 ms; sensores são lidos aproximadamente a cada segundo. O GTK verifica estado a cada 100 ms e redesenha quando necessário. Conserva o último quadro se faltar comunicação e marca pausa. Encerra com o proprietário e confere starttime para evitar reutilização do PID. Alt+Q usa conexão X11 separada, com limitações do grab/compositor em Wayland.

Controles são agrupados por debounce de 120 ms e enviados ao serviço sem reiniciar o processo GTK. Frametime usa leitura incremental limitada de CSV com coluna `frametime`; amostras ausentes/antigas não são substituídas por temporização da telemetria. Consulte [OVERLAY.md](OVERLAY.md).

Formato lista/compacto/colunas, largura/fonte/fundo/campos e posicionamento salvos. Âncoras: quatro cantos, superior centralizado, centro ou coordenadas; o cálculo considera monitores com coordenadas negativas e limita a janela à sua área. Índices de monitor podem diferir entre Qt/Wayland e GTK/XWayland; exigem conferência na sessão real.

## GPU e sensores

BDF PCI identifica o dispositivo; não presume GPU0/card0/renderD128. NVML é carregada dinamicamente. APIs modernas de temperatura e RPM são tentadas quando disponíveis; ausência de um sensor não se transforma em zero. Percentual de ventoinha não é convertido artificialmente para RPM. A API RPM da NVIDIA informa a velocidade pretendida pelo driver, que pode diferir da física. Hwmon fornece os canais que o kernel realmente expõe.

NVIDIA, AMD e Intel têm capacidades distintas. O painel informa N/D para dados ausentes e diferencia bibliotecas indisponíveis, render nodes/permissões e capacidades não expostas. Intel fdinfo mede somente clientes acessíveis do usuário, com deduplicação por client ID. Não equivale a atribuição perfeita de uso ao jogo selecionado.

## Testes e persistência

Os testes gráficos usam outro executável, sem operação para escrever em clocks/tensão/potência. Vulkan usa dispositivo físico identificado e imagem offscreen de tamanho fixo; OpenGL usa contexto/framebuffer e verifica o renderer/PCI quando exposto. FP64/int64 dependem de features consultadas. O worker valida amostras computacionais e comunica progresso/resultados por linhas JSON.

FFmpeg roda sem shell, com GPU/render node explícito. A suíte separa preparação e medição, exige frames de hardware e registra falhas/incompatibilidades. A interface pode cancelar processos, e o watchdog encerra renderizações sem progresso.

SQLite usa a API C diretamente, sem plugin QSQLITE. Cada categoria mantém os três últimos resultados na interface por GPU; os demais registros continuam na base. Importações são transacionais, sanitizadas, deduplicadas e marcadas como não verificadas. Não são usadas para aplicar clocks.

## Privilégios

O helper `gpu-clock-helper` usa o arquivo-fonte de nome legado `src/power_helper.cpp`, dedicado exclusivamente ao controle de clocks. Não contém setter de tensão ou limite de potência. Aceita apenas PCI validado, domínio conhecido e número dentro das capacidades consultadas. O teste curto NVIDIA limita a variação a +15 MHz e restaura o core original, preservando memória e power limit. Usa lock por dispositivo e comunicação stdin/EOF, com limite temporal e térmico.

Executável instalado root-owned 0755, sem setuid, serviço, sudoers ou regra polkit ampliada. Cada uso depende da autorização do sistema. A interface inteira permanece como usuário comum. Nenhum ajuste é aplicado na abertura.

## Referências primárias

- [Qt para Linux](https://doc.qt.io/qt-6/linux.html)
- [NVML: consultas](https://docs.nvidia.com/deploy/nvml-api/latest/api/group__nvmlDeviceQueries.html)
- [AMDGPU: energia e clocks](https://www.kernel.org/doc/html/latest/gpu/amdgpu/thermal.html)
- [DRM fdinfo](https://www.kernel.org/doc/html/latest/gpu/drm-usage-stats.html)
- [MangoHud](https://github.com/flightlessmango/MangoHud)
- [vkBasalt](https://github.com/DadSchoorse/vkBasalt)
- [Gamescope](https://github.com/ValveSoftware/gamescope)
- [FFmpeg](https://ffmpeg.org/ffmpeg.html)


## Interface e atualização

GPU agrupa monitor/ficha; Benchmark agrupa quatro categorias. Conteúdos usam QScrollArea com tamanho mínimo de layout e rolagem vertical. NeonCard repinta o fundo integralmente e desenha bordas animadas apenas quando visível e com animação habilitada; valores continuam sendo os sensores originais. O estilo Fusion e a paleta são fixos, sem alterar o tema global. A saída explícita da bandeja encerra o event loop após um fechamento aceito; uma janela oculta não depende do sinal lastWindowClosed.

A prévia 3D é um JPEG atômico em QTemporaryDir, anunciado pelo worker com FRAME. A janela recebe quadros sem bloquear o loop de renderização/telemetria. A carga principal permanece offscreen e usa timestamps da GPU.

UpdateManager usa Qt Network e libarchive. Consulta manual de releases; valida origem, tamanho, hash, manifesto e caminhos; backup antes de escrever; QSaveFile por arquivo e tentativa de rollback em falha. Não altera dados de testes ou helper root. O canal de atualização é separado do feedback, que continua local e sem upload.
