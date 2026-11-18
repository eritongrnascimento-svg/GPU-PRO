# Configurar o overlay

Em **GamerMonitor & Overlay**, ative **Exibir HUD** e escolha os campos, formato, monitor, posição, fonte, largura e fundo. As mudanças são aplicadas automaticamente após uma pequena pausa de aproximadamente 120 ms; não é necessário desativar e reativar o HUD. O botão Aplicar continua disponível. A configuração é salva localmente.

O HUD usa um processo separado em GTK/X11 e recebe snapshots por UDP local. Pode continuar visível quando o painel está na bandeja. Encerrar o aplicativo também encerra seu overlay. O comportamento sobre jogos, fullscreen e XWayland depende do compositor; o HUD não captura cliques. Alt+Q alterna sua visibilidade quando o atalho está disponível.

## Uso e temperatura da CPU

Marque **Uso CPU** e **Temperatura CPU** na lista de métricas. O uso é global, calculado por diferenças dos contadores de `/proc/stat`, e não o uso do processo acompanhado. A primeira leitura precisa de duas amostras; até lá aparece N/D. A coleta ocorre aproximadamente uma vez por segundo.

Temperaturas são lidas de sensores CPU reconhecidos em hwmon, como coretemp, k10temp e zenpower, com fallback para zonas térmicas identificadas como CPU/pacote/SoC. Tdie e pacote têm preferência; quando há canais equivalentes, é exibido o maior valor válido. A origem permanece na telemetria local. Se o kernel não expuser um canal reconhecido, aparece N/D. Não é estimada a temperatura da CPU a partir da GPU.

## FPS do jogo (contador do GPU-PRO)

Desde a 0.8.1 o FPS vem do contador próprio do GPU-PRO, sem MangoHud nem PresentMon:

- **Linux:** clique uma vez em **Ativar FPS automático** (GamerMonitor & Overlay). A camada Vulkan do GPU-PRO (`VK_LAYER_GPUPRO_fps`) passa a contar os quadros de jogos Vulkan nativos e de todos os jogos pelo Proton (DirectX 9–11 via DXVK, DirectX 12 via VKD3D-Proton). O manifesto fica em `~/.local/share/vulkan/implicit_layer.d/gpu-pro-fps.json`; a camada só registra o instante de cada `vkQueuePresentKHR` e envia os tempos ao GPU-PRO por um socket local. Jogos já abertos passam a contar quando forem reiniciados. Para desativar, use o mesmo botão ou `DISABLE_GPUPRO_FPS=1` em um jogo.
- **OpenGL nativo (Linux):** nas opções de inicialização do Steam, use `gpu-pro-fps %command%`. O lançador também ativa a camada Vulkan para aquele jogo, mesmo sem o FPS automático.
- **Windows:** veja [Windows](WINDOWS.md#fps-dos-jogos).

Sem processo escolhido, o jogo é detectado sozinho; compositores, gravação de tela, navegadores e sobreposições são ignorados. O HUD mostra o nome do jogo e a API.

As estatísticas usam os últimos 240 quadros. FPS atual = 1000 / último frametime em ms; média = 1000 × quantidade / soma dos frametimes; máxima = 1000 / menor frametime. O 1% low usa a média dos tempos do 1% mais lento e só aparece após 100 quadros. Sem quadros novos por cerca de 3 s, o FPS fica N/D. Nenhum FPS é inferido do uso da GPU.

### CSV opcional

Um CSV de frametime em gravação (coluna `frametime` em ms, ou `fps`) escolhido em **Selecionar CSV** tem prioridade sobre o contador — útil para logs do MangoHud ou de outras ferramentas. Logs por intervalo geram estatísticas das amostras, não por quadro.

## Tema e gravação

O aplicativo fixa o estilo Qt Fusion e sua paleta para impedir que Kvantum ou o tema Qt da sessão alterem os controles. Essa escolha vale para o GPU-PRO, sem modificar o tema global ou o ambiente de jogos iniciados pelo painel.

Os cards animados repintam o fundo integralmente. Em **Atualizações**, a opção **Animar luzes neon dos cards** permite desligar o movimento, mantendo cores e valores atualizados. Use-a caso queira reduzir movimento/custo de repintura durante gravações. Isso não promete corrigir falhas externas do driver, compositor ou captura do OBS.
