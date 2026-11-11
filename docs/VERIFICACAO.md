# Registro de validação do GPU-PRO

## Versão 0.6.1

- Nove verificações locais aprovadas, preservando a suíte anterior e acrescentando ativação/recuperação de instância única e transparência/cores dos ícones.
- O teste de instância única inicia processos separados em pasta temporária, confirma a resposta da instância existente, encerra o dono de teste e verifica a recuperação e o isolamento entre sessões. Requer permissão para sockets locais; no ambiente de desenvolvimento com sandbox essa verificação foi executada fora da restrição de rede.
- Teste adicional com o executável real e o AppImage, em configuração/sessão temporárias, confirmou que as duas segundas aberturas ativam o painel existente e encerram sem manter outra instância. Apenas o processo de teste foi encerrado.
- A interface offline valida que mostrar uma janela oculta preserva seu estado maximizado. Saída da bandeja e interações de estilo continuam cobertas.
- Renderização dos ícones conferida em verde, vermelho e azul, incluindo tamanhos 16/24/32 px. A variação dos quadros do halo e a transparência externa são verificadas sem sensores ou alterações da GPU.
- Comportamento de foco/decoração e animação efetiva da bandeja dependem do compositor e ainda precisam ser observados na sessão gráfica do usuário. Nenhuma instância antiga do usuário foi encerrada à força; não houve instalação de driver, ajuste de clocks ou mudança de áudio nesta atualização.

## Versão 0.6.0

- Sete verificações locais ampliadas para conjuntos completos de drivers, parsing integral de adições/remoções, contagem de EUs Intel com fixtures, configuração/desfazer da sessão em diretórios temporários, classificação de áudio virtual/físico, atualização/restauração AppImage e validação/staging DEB sem instalação.
- Consulta real aos índices CachyOS encontrou 11 conjuntos NVIDIA completos antes do filtro de arquitetura na interface, em vez da lista de componentes avulsos.
- Simulação real de troca para NVIDIA 580 Open: sete pacotes a adicionar/reinstalar e seis a remover. Realizada pelo pacman externo em namespace sem rede, com sistema somente de leitura e confirmação final recusada. Nenhum driver foi trocado.
- Não há vinculação à biblioteca libalpm. A integração utiliza programas nativos externos.
- Troca efetiva de drivers, roteamento real de áudio e preferência real da sessão não foram aplicados no CachyOS durante esta atualização. Intel física, ambas as GPUs ativas e comportamento posterior ao reboot ainda exigem validação controlada.


## Versão 0.5.0

- Sete verificações locais: núcleo/CPU/CSV, rejeição de escrita de tensão/potência, atualização/backup/restauração, layout, saída de janela oculta e interações de HUD/estilo/aceite.
- Reproduzido o caso Qt em que `close()` de janela oculta retorna aceito, mas não encerra o event loop; saída explícita corrige esse comportamento.
- Testes de CPU usam contadores controlados e sensores fictícios, sem contar guest duas vezes. Uma coleta real no ambiente CachyOS também retornou uso global e temperatura CPU por hwmon.
- CSV: descoberta de cabeçalho, anexação, linhas incompletas, valores inválidos, truncamento e ausência de duplicação. O gráfico é validado com dados de teste identificados como prévia fictícia, sem fingir medição de jogo.
- Teste da interface com `QT_STYLE_OVERRIDE=kvantum` confirma estilo Fusion, debounce/configuração ao vivo e confirmação de risco inicialmente desabilitada. Recusar não concede acesso; marcar a caixa habilita o aceite.
- Nenhum overclock ou instalação de driver foi executado nesta atualização. Artefatos específicos de OBS/compositor em uma sessão real e frametime em jogos ainda exigem validação física; os testes de estilo não certificam ausência de toda falha de captura.

## Versão 0.4.0 — pacotes publicados

Compilação, testes e geração dos formatos concluídos no [workflow de pacotes](https://github.com/eritongrnascimento-svg/GPU-PRO/actions/runs/42852585433). Os oito arquivos publicados tiveram tamanho e SHA-256 conferidos; AppImage e DEB extraído passaram na verificação local de interface, e RUN passou na verificação de integridade. O DEB não foi instalado durante esse protocolo.

### Verificações de recursos 0.4

- Cinco verificações locais aprovadas, incluindo percentuais CPU/GPU, validação de versões de drivers e alcance da rolagem.
- Layout adaptável das especificações verificado em 1240×900, com fallback de rolagem em janela menor.
- Consulta real ao pacman/CachyOS retornou 74 opções de componentes NVIDIA; versões disponíveis e instaladas foram diferenciadas. Não foi executada instalação/atualização de drivers.
- Ensaios curtos Vulkan com CPU sintética em 720p e 1080p concluídos. A estimativa em 1080p indicou CPU predominante, cerca de 40,3% relativo ao potencial GPU no modelo. Isso não é perda real de desempenho de jogos.
- Nenhuma escrita de tensão, power limit ou clocks nesta etapa; helper root existente preservado.
- Compilação/testes Ubuntu e instaladores são produzidos pelo workflow Pacotes Linux; consulte as execuções e os arquivos finais da release.

## Histórico — GPU-PRO 0.3.0

## Atualização 0.3.0 — 10/11/2026

CI inicial no Ubuntu 24.04: compilação e testes offline aprovados no [GitHub Actions](https://github.com/eritongrnascimento-svg/GPU-PRO/actions/runs/42824679190). Essa execução antecede o acréscimo dos avisos/licenças na interface; a execução de empacotamento da versão 0.4, vinculada abaixo, cobre uma revisão posterior.

Ambiente local: CachyOS x86_64, Qt 6.12, KDE Wayland/XWayland, RTX 3060 Ti, driver 620.70.06.

- Compilação concluída e **5/5 testes CTest aprovados**: núcleo/sensores/histórico, rejeição de tensão, rejeição de potência, atualização/backup/restauração e layout/rolagem.
- Interface: 10 menus, duas abas GPU e quatro abas Benchmark. Verificação em 980×700 confirmou que a rolagem alcança o final da tabela do GamerMonitor. Capturas revisadas com tema escuro e cards neon.
- Vulkan 720p: 30 s medidos + 2 s de aquecimento com nova cena e prévia contínua; 374 quadros de prévia publicados, máxima de 63 °C.
- Fluxo completo da janela: ensaio de 6 s, 99 quadros de prévia recebidos, cena exibida e resultado persistido no histórico.
- OpenGL 720p: ensaio curto de 3 s + aquecimento com novo shader/prévia, resultado válido. Não foi um ensaio completo de 30/60 s.
- Ferramentas Python opcionais instaladas no usuário, sem virtualenv: gpustat 1.1.1, nvidia-ml-py 13.615.71; psutil 7.2.2 já disponível. NVML field 82 retornou NOT_SUPPORTED nesta RTX 3060 Ti. Não foi localizada leitura pública de hotspot neste driver.
- Fixture hwmon verificou edge=46 °C, hotspot/junction=67 °C e memory junction=78 °C sem confundir memória com hotspot; remover o canal junction mantém hotspot indisponível.
- Atualizador: instalação/backup/restauração do pacote completo final (tar.gz) em diretórios temporários, além das fixtures; rejeição de checksum incorreto, versão incompatível, links, travessia de caminhos e atualização durante testes. Naquele protocolo, a atualização por download de uma versão posterior ainda não foi exercitada; isso permanece distinto dos testes locais de instalação e restauração.

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

No ambiente de validação, o auxiliar foi instalado em `/usr/local/libexec/linux-gpu-control/gpu-clock-helper`, propriedade root, modo 0755, sem setuid, serviço ou início automático. Novos ajustes continuam exigindo autenticação do sistema.

Esse ensaio curto não comprova estabilidade prolongada nem ganho significativo de desempenho. Não gera recomendação para outras placas.

## Limites desta validação

AMD e Intel ainda precisam de testes nos computadores dos participantes. As demais resoluções e combinações de driver também não foram todas testadas. A temperatura de memória aparece indisponível quando o driver não a fornece; nesta placa, a consulta NVIDIA também retornou N/A. Fan parado pode corretamente reportar 0 RPM.

Não foram validados jogos, filtros/upscalers em títulos específicos, fullscreen exclusivo ou overclock prolongado. As prévias genéricas de temas e HUD podem usar dados fictícios; os exemplos em `examples/` identificam os resultados reais.
