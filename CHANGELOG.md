# Alterações

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
