# Alterações

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
