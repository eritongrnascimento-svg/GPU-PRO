# Alterações

## 0.8.1 — 18/11/2026 · patch de correção (Linux e Windows)

Corrige o FPS dos jogos no HUD da 0.8.0 e torna os resultados do Benchmark mais claros. Detalhes em `docs/RELEASE-0.8.1.md`.


- FPS dos jogos sem programas externos. Windows: contador próprio por eventos do Windows (DirectX 9–12, Vulkan e OpenGL), serviço "GPU-PRO · Contador de quadros" instalado uma vez; PresentMon removido das dependências. Linux: camada Vulkan própria (Vulkan nativo e Proton/DXVK/VKD3D) ativada uma vez e lançador `gpu-pro-fps` para OpenGL; sem MangoHud. Jogo detectado automaticamente; CSV manual continua com prioridade.
- Benchmark: aba Teste completo (sequência com pontuação total e por categoria); 3D com método resumido, pontuação e FPS mínimo/médio/máximo; memória com transferências CPU ↔ GPU e pontuações de memória e computação; codecs de vídeo em 720p, 1080p, 1440p e 4K e teste de imagens JPEG/PNG/WebP, com pontuação.
- Notificações & Logs: botão Abrir trecho para cada resultado da busca, com o registro completo e as linhas ao redor.
- Feedback & Diagnóstico: removidos Importar feedback de amigos e Consultar banco importado.
- Windows: cor da interface pela GPU selecionada ou conectada (NVIDIA verde, AMD vermelho, Intel azul), sem alterar o tema do Windows; soma do Teste completo corrigida (dava 0 pontos).
- Ficha técnica: uma linha por codec e modo com as resoluções suportadas; no Windows, os codecs repetidos por resolução deixavam a ficha em N/D e fechavam o app.
- GitHub Actions: ações em Node.js 24 e tempo limite maior para o empacotamento Linux.

## 0.8.0 (Linux) — 17/11/2026

Design igual ao da versão Windows, respeitando as funções do Linux.

- Marca GPU-PRO com ícone, menu na mesma ordem do Windows e aba Sobre (cafezinho por PIX e versículo, também na barra lateral). Janela abre centralizada (X11; no Wayland nativo a posição é do compositor).
- Notificações & Logs em uma aba só, com busca, filtro por categoria e cópia diária em `~/.config/.../logs`; mensagens não aparecem mais em outras abas nem na barra de status.
- Overclock: termo de responsabilidade versão 2, gráfico de estabilidade (clock, memória, temperatura, potência, ventoinha e uso; marcas dos ajustes; diagnóstico sob carga; CSV) e página compactada.
- Gargalo: resultado em "GPU, CPU e memória perdendo X% de desempenho"; memória só perde com a VRAM no limite e o quadro mais lento que o esperado pela resolução (na AMD, o GTT confirma o transbordo para a RAM).
- Monitores & Telas: além do modo, rotação e monitor principal (xrandr e KDE), escala, HDR, VRR e perfil de cor (KDE Wayland, kscreen-doctor) e brilho/contraste por DDC/CI (ddcutil), com reversão em 15 s para geometria e HDR; um único botão abre as configurações de tela do sistema.
- 3D & Filtros: recursos suportados pela GPU e bibliotecas instaladas (ray tracing Vulkan/DX12 via VKD3D, DLSS/Frame Generation por arquitetura, FSR, XeSS, Gamescope, vkBasalt, MangoHud, GameMode) e novas opções do perfil: filtro FSR/NIS e nitidez, HDR e VRR no Gamescope; DLSS via DXVK-NVAPI, ray tracing DX12 e HDR no Proton; GameMode.
- HUD: leituras de RAM, clock da CPU e disco (leitura/escrita, tempo ativo e temperatura NVMe/SATA), cores de fonte, título e gráfico e modo área de trabalho (abaixo das janelas).
- Correções: títulos com "&" apareciam como "_"; janela do HUD renomeada para "GPU-PRO HUD"; exportação de imagem com "GPU-PRO · Gargalo" em vez de "GPU CONTROL · BOTTLENECK".

## 0.8.0 (Windows) — 17/11/2026

Primeira versão nativa para Windows (WinUI 3 + núcleo C++ compartilhado com o Linux). Instalador (Inno Setup, por usuário, sem administrador) e pacote portátil ZIP; detalhes em `docs/WINDOWS.md`.

- Termo de responsabilidade e assunção de riscos (versão 2) ao abrir Overclock, no Windows e no Linux: danos ao equipamento, instabilidade, perda de dados e garantia, com base no CF art. 5º, II, CC arts. 186, 927 e 945, CDC art. 12, § 3º, III, e art. 14, § 3º, II, e Lei 9.609/1998; aceite obrigatório, opção de não exibir novamente na mesma versão, registro local e no log (`docs/AVISO-DE-RESPONSABILIDADE.md`).
- Overclock: gráfico detalhado de estabilidade (clock, memória, temperatura, potência, ventoinha e uso nos últimos 10 minutos), marcas dos ajustes aplicados, diagnóstico sob carga (Estável / Oscilação leve / Instável, com causa provável: temperatura ou limite de potência) e exportação CSV. Página compactada.
- Correção: o app fechava sozinho ao trocar de aba (GamerMonitor & Overlay e botão Exibir HUD): controles reaproveitados eram reinseridos antes de o WinUI soltá-los da página anterior. Agora são recriados a cada montagem; validado com 112 trocas de aba e 16 ciclos do HUD.
- Aba Sobre: projeto, repositório, "cafezinho" por PIX (QR estático no padrão BR Code, gerado por `tools/pix-qr.py`, e copia e cola) e versículo sorteado (Almeida Revista e Corrigida), também exibido no rodapé da barra lateral.
- A janela sempre abre centralizada na tela principal.

- Nome GPU-PRO e ícone da barra de tarefas também na barra de título.
- GPU: cards do monitor se ajustam à janela sem rolagem; ficha técnica no mesmo formato da versão Linux (DXGI, NVML e Vulkan associado pelo LUID: arquitetura, unidades, barramento, clocks máximos, limites de potência, PCIe atual/máximo, VBIOS, ventoinhas, ray tracing e Vulkan Video).
- Correção: leituras de encoder, decoder, memória compartilhada e uso da GPU (AMD/Intel) pelos contadores do Windows não eram associadas à GPU por diferença de maiúsculas no identificador.
- Configurações 3D & Filtros: recursos suportados (DXR, DirectX 12 Ultimate, mesh shaders, VRS, shader model, DLSS SR/RR/FG/MFG, NIS, FSR, XeSS); configurações 3D do driver NVIDIA no perfil global (override de DLSS, Image Scaling, V-Sync, quadros pré-renderizados, limite de FPS, energia, filtragem, VRR), com autorização do Windows e confirmação por leitura; Auto HDR, VRR, otimizações para jogos em janela, Modo de Jogo, agendamento de GPU por hardware e GPU por jogo.
- GamerMonitor & Overlay: HUD com fundo translúcido e desfoque, no jogo ou na área de trabalho; escolha de cada leitura de FPS/frametime, GPU, CPU, memória e disco (leitura/escrita, tempo ativo e temperatura); formato lista, duas colunas ou faixa; cores de fonte, título e gráfico; largura, fonte, fundo, deslocamento, posição e monitor. Página compactada.
- Monitores & Telas: resolução, frequência e orientação com confirmação de 15 s, escala, HDR, monitor principal, brilho/contraste por DDC/CI e perfil de cor dentro do app.
- Benchmark de gargalo: resultado em linguagem direta — GPU, CPU e memória perdendo X% de desempenho; memória só perde quando a VRAM transborda para a RAM do sistema.
- Notificações & Logs: aba única; nenhuma outra aba exibe logs. Log de operações com busca e filtro por categoria, cópia diária em `%LOCALAPPDATA%\GPU-PRO\logs`.
- Empacotamento reproduzível: `windows/build-windows.ps1` (CMake, windeployqt, dotnet publish, runtime C++, testes, ZIP e SHA-256).

## 0.7.0 — 13/11/2026

- Overclock sem presets: clocks manuais, power limit em W, offset independente de tensão/undervolt AMD e ventoinhas conforme suporte do driver. Nenhum ajuste é aplicado ao iniciar.
- NVIDIA usa NVML para potência e ventoinhas por índice. AMD amdgpu usa hwmon e a interface OD_VDDGFX_OFFSET reconhecida. Intel e interfaces não reconhecidas permanecem indisponíveis. Não implementa curva V/F ou tensão absoluta NVIDIA.
- Confirmação e autenticação do sistema, limites consultados novamente no helper, confirmação por leitura e tentativa de restauração em falhas. Botão para retornar as ventoinhas ao automático.
- FPS atual, média, máxima e 1% low das amostras CSV selecionáveis no GamerMonitor e HUD. Janela móvel de até 240 amostras; low exige pelo menos 100. CSV por intervalo não equivale a estatística de cada quadro.
- Gráfico de frametime no GamerMonitor; coleta independente de habilitar o desenho no HUD. Fonte inativa por mais de 3 s limpa dados; mudança de arquivo ou novo cabeçalho reinicia a série.
- Ícone PNG e SVG empacotado com o mesmo canal alpha. Ícone de janela/barra sem halo colorido, igual nos três temas. A bandeja GP conserva sua animação.
- DEB usa o botão Baixar no GitHub; atualização interna de DEB fica desativada na interface. AppImage e TAR/RUN mantêm o atualizador existente.
- Validação Linux no Ubuntu 26.04 WSL; testes offline não certificam controle físico da GPU nem todos os compositores.

- Correção no WSL: descoberta NVIDIA via NVML sem depender do adaptador DRM vgem; sensores reais e ficha com os dados disponibilizados pelo driver Windows. Escritas bloqueadas no WSL.
- Lançadores Windows com Wayland e janela maximizada; fontes Noto instaladas no ambiente de teste. Validação ampliada: 12/12 testes e capturas reais de sensores/ficha.

Pacotes oficiais recompilados em Ubuntu 24.04 x86_64; testes locais de WSL no Ubuntu 26.04 são uma validação separada.

## 0.6.1

- Novo ícone fornecido para o projeto; transparência externa e contorno neon dinâmico na janela.
- Bandeja com monograma GP e halo animado, pausa no menu e preferência para desativar a animação.
- Instância única por usuário/sessão gráfica, ativação da janela existente e recuperação de trava após encerramento inesperado.
- Nome de ícone próprio no desktop e atualização de caches no DEB; identidade da aplicação alinhada ao atalho.
- Prévias e testes offline isolados da instância normal; documentação da integração e das limitações do compositor.

## 0.6.0

- Seleção de conjuntos completos de drivers e versão instalada separada; prévia de adições/remoções e restauração validada.
- Atualização pelo painel de AppImage e DEB, com backup e manutenção do link para GitHub.
- Integrações de pacotes por comandos externos, sem vinculação à libalpm ou a bibliotecas exclusivamente GPL no código original.
- Especificações Intel ampliadas com dados DRM/i915 e memória compartilhada identificada.
- Preferência de GPU para novos aplicativos e configuração reversível da sessão KDE Wayland.
- Alinhamento/restauração de destinos de áudio sem reiniciar serviços nem mover capturas existentes.
- Ícone gpu-pro.png atualizado; recursos e históricos anteriores preservados.

## 0.5.0

- Corrigida a saída pela bandeja com a janela já oculta; teste de regressão incluído.
- Uso global e temperatura de CPU no overlay, com leitura Linux e ausência explícita de sensores.
- Controles do HUD aplicados ao vivo; linha de frametime com leitura incremental de CSV ativo compatível.
- Estilo Qt Fusion e paleta fixos, repintura integral dos cards e opção de pausar a animação neon.
- Aviso de riscos na documentação e caixa de aceite obrigatória por sessão para acessar Overclock.
- Documentação revisada para visitantes e colaboradores, preservando informações técnicas e histórico.

## 0.4.0

- Cards de especificações adaptáveis à janela, com fallback de rolagem.
- Resolução alvo e percentuais estimados CPU/GPU no teste de gargalo, preservando registros anteriores.
- Opção de manter o painel na bandeja, com mostrar e sair.
- Consulta/simulação de versões de drivers e execução pelo gerenciador da distro na aba Atualizações.
- Pacotes .deb, AppImage e executável .run; diretório principal renomeado para GPU-PRO.


## 0.3.0 — GPU-PRO

- Novo nome e identificação do titular: Ériton do Nascimento.
- Primeira página GPU com abas Monitor de recursos e Especificações; Dashboard removido do menu.
- Rolagem nas páginas e subabas, incluindo o bloco Métrica do dispositivo do GamerMonitor.
- Benchmark unificado: 3D/Jogos, Computação, Encoder/Decoder e Gargalo.
- Cards com bordas neon animadas e cores por faixa de leitura.
- Três cenas 3D procedurais neon, câmera animada, sombras e reflexos; durações de 30/60 s e prévia ao vivo.
- Método 3D `neon-throughput-v0.3`; scores não equivalem aos da versão 0.2. Prévia identificada no JSON.
- Sensores: memória NVML field 82 quando suportada; correção da classificação de memory junction em hwmon; origem/indisponibilidade explícitas do hotspot.
- Consulta manual a GitHub Releases, verificação de pacote, backup e restauração. Nenhuma release remota é presumida existente.
- Licença própria source available, política de contribuições e documentação de instalação/publicação.

## 0.2.0

Benchmarks reais OpenGL/Vulkan, computação, suíte sequencial de vídeo, históricos SQLite, exportações, feedback sanitizado e teste temporário conservador de core NVIDIA com restauração.
