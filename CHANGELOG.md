# Alterações

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
