# Atualização 0.4 — verificações

- Cinco verificações locais aprovadas, incluindo percentuais CPU/GPU, validação de versões de drivers e alcance da rolagem.
- Layout adaptável das especificações verificado em 1240×900, com fallback de rolagem em janela menor.
- Consulta real ao pacman/CachyOS retornou 74 opções de componentes NVIDIA; versões disponíveis e instaladas foram diferenciadas. Não foi executada instalação/atualização de drivers.
- Ensaios curtos Vulkan com CPU sintética em 720p e 1080p concluídos. A estimativa em 1080p indicou CPU predominante, cerca de 40,3% relativo ao potencial GPU no modelo. Isso não é perda real de desempenho de jogos.
- Nenhuma escrita de tensão, power limit ou clocks nesta etapa; helper root existente preservado.
- Compilação/testes Ubuntu e instaladores são produzidos pelo workflow Pacotes Linux; consulte as execuções e os arquivos finais da release.

# Registro de validação — GPU-PRO 0.3.0

## Atualização 0.3.0 — 10/11/2026

CI inicial no Ubuntu 24.04: compilação e testes offline aprovados no [GitHub Actions](https://github.com/eritongrnascimento-svg/GPU-PRO/actions/runs/42824679190). Essa execução antecede o acréscimo dos avisos/licenças na interface; a revisão final deve ser verificada novamente.

Ambiente local: CachyOS x86_64, Qt 6.12, KDE Wayland/XWayland, RTX 3060 Ti, driver 620.70.06.

- Compilação concluída e **5/5 testes CTest aprovados**: núcleo/sensores/histórico, rejeição de tensão, rejeição de potência, atualização/backup/restauração e layout/rolagem.
- Interface: 10 menus, duas abas GPU e quatro abas Benchmark. Verificação em 980×700 confirmou que a rolagem alcança o final da tabela do GamerMonitor. Capturas revisadas com tema escuro e cards neon.
- Vulkan 720p: 30 s medidos + 2 s de aquecimento com nova cena e prévia contínua; 374 quadros de prévia publicados, máxima de 63 °C.
- Fluxo completo da janela: ensaio de 6 s, 99 quadros de prévia recebidos, cena exibida e resultado persistido no histórico.
- OpenGL 720p: ensaio curto de 3 s + aquecimento com novo shader/prévia, resultado válido. Não foi um ensaio completo de 30/60 s.
- Ferramentas Python opcionais instaladas no usuário, sem virtualenv: gpustat 1.1.1, nvidia-ml-py 13.615.71; psutil 7.2.2 já disponível. NVML field 82 retornou NOT_SUPPORTED nesta RTX 3060 Ti. Não foi localizada leitura pública de hotspot neste driver.
- Fixture hwmon verificou edge=46 °C, hotspot/junction=67 °C e memory junction=78 °C sem confundir memória com hotspot; remover o canal junction mantém hotspot indisponível.
- Atualizador: instalação/backup/restauração do pacote completo final (tar.gz) em diretórios temporários, além das fixtures; rejeição de checksum incorreto, versão incompatível, links, travessia de caminhos e atualização durante testes. O download/upgrade a partir de uma release posterior real ainda depende dessa publicação futura.

Nesta atualização não foi executado novo overclock nem alterado o helper root já instalado. Não foram escritos tensão, limite de potência ou clocks durante os ensaios 0.3.0. A nova carga 3D não é comparável à versão 0.2.0. Os resultados de 8K, codecs e computação abaixo pertencem à validação anterior, não foram repetidos com a nova cena.

## Registro anterior — 0.2.0


Validação em 10/11/2026, Linux x86_64, KDE Wayland/XWayland, NVIDIA RTX 3060 Ti (8 GiB), driver 620.70.06. C++20 / Qt 6 / GTK3 / Vulkan / SQLite3.

## Verificações concluídas

- Compilação de interface, workers, helper e shaders; CTest: 3/3 passaram (núcleo offline, rejeição de escrita de potência e de tensão).
- Interface offline: 12 páginas, históricos e relatórios de resultados reais; exportação PNG/JPEG e feedback JSON. Inspeção visual das telas e da cena OpenGL.
- Vulkan 720p e 8K (7680×4320) e OpenGL 720p: renderização real offscreen, GPU verificada, três cenas e pontuação em MPix/s geométrico. São ensaios curtos de execução, não comparação estatística de desempenho.
- Computação Vulkan: leitura, escrita, cópia, FP32/FP64 e inteiros 32/64 bits; sete medições com validação das saídas.
- Suíte de vídeo: H.264 encode/decode, HEVC encode/decode e AV1 decode concluídos por hardware. AV1 encode não suportado nesta RTX 3060 Ti; cobertura parcial explícita, sem zero fictício.
- Gargalo sintético: comparação 720p/1440p com tempos de CPU/GPU; resultado restrito à carga utilizada.
- Feedback: sete registros reais sanitizados no exemplo, persistência SQLite, deduplicação e validação de importação. Não inclui caminhos locais, UUID da placa ou logs brutos.
- Overlay: geometria da opção Superior centralizado testada; renderização offline. Comportamento sobre jogos e fullscreen ainda exige teste em jogo.

## Teste autorizado de clocks

O fluxo completo da interface mediu uma execução original, aguardou resfriamento/ociosidade, solicitou autenticação pelo sistema, aplicou temporariamente +15 MHz no core e executou a comparação curta. O helper registrou o ajuste aplicado e confirmou a restauração para 0 MHz. Máxima registrada no fluxo completo: **60 °C**.

Offset de memória: 0 antes e depois. Limite de potência: 225000 mW (225 W) antes e depois. Não houve comando de alteração de tensão ou potência. O helper possui watchdog independente e restauração ao término, cancelamento ou perda da interface.

O auxiliar aprovado foi instalado em `/usr/local/libexec/linux-gpu-control/gpu-clock-helper`, propriedade root, modo 0755, sem setuid, serviço ou início automático. Novos ajustes continuam exigindo autenticação do sistema.

Esse ensaio curto não comprova estabilidade prolongada nem ganho significativo de desempenho. Não gera recomendação para outras placas.

## Limites desta validação

AMD e Intel ainda precisam de testes nos computadores dos participantes. As demais resoluções e combinações de driver também não foram todas testadas. A temperatura de memória aparece indisponível quando o driver não a fornece; nesta placa, a consulta NVIDIA também retornou N/A. Fan parado pode corretamente reportar 0 RPM.

Não foram validados jogos, filtros/upscalers em títulos específicos, fullscreen exclusivo ou overclock prolongado. As prévias genéricas de temas e HUD podem usar dados fictícios; os exemplos em `examples/` identificam os resultados reais.
