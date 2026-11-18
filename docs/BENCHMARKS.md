# Métodos da versão 0.8.1

## Teste completo: `suite-v1`

Sequência fixa: cenas 3D (Vulkan, 1920×1080, 30 s), memória e computação da GPU, codecs de vídeo e de imagem e processador. Cada etapa é salva também no histórico da própria categoria. Pontuação total = soma das pontuações das categorias. Cancelar interrompe a sequência sem gravar pontuação total.

## Pontuações simplificadas

- **3D:** média geométrica dos megapixels desenhados por segundo nas três cenas × 10. FPS mínimo e máximo são os piores e melhores segundos do teste (quadros concluídos por segundo de relógio); FPS médio = quadros / tempo medido.
- **Memória da GPU:** média geométrica, em GB/s, de leitura, escrita e cópia na VRAM por shader, envio CPU → GPU e retorno GPU → CPU pelo PCIe (`vkCmdCopyBuffer` entre memória visível pela CPU e VRAM) e cópia direta na VRAM; × 10. 32 MiB por medição, 8 repetições cronometradas pela GPU; o envio é validado na volta.
- **Computação da GPU:** média geométrica de GFLOPS (FP32, FP64) e GIOPS (inteiros de 32 e 64 bits) suportados.
- **Codecs de vídeo (`codec-suite-v0.3`):** H.264, HEVC e AV1 por hardware, codificação e decodificação em 1280×720, 1920×1080, 2560×1440 e 3840×2160, 360 quadros cada; média geométrica dos megapixels por segundo × 10. Um codec não suportado na primeira resolução não é repetido nas demais.
- **Codecs de imagem:** JPEG (qualidade 90), PNG e WebP (quando o Qt oferece) pelo processador, codificação e decodificação nas mesmas quatro resoluções, ao menos 1,2 s por item; média geométrica dos megapixels por segundo × 100.
- **Processador:** inteiros, FP64 e primos do núcleo compartilhado, 5 s cada em todas as threads; média geométrica das operações por segundo ÷ 10 000.

As escalas são próprias do GPU-PRO; não equivalem a pontos de outros benchmarks. Compare a mesma versão de método.

# Métodos da versão 0.3

## 3D: `neon-throughput-v0.3`

Três cenas procedurais: cidade neon, núcleo cristalino e corredor de anéis. Câmera animada, iluminação/sombras, oclusão e reflexos por ray marching em shaders. Não é um jogo comercial e não usa unidades RT dedicadas. Mesmo código-base de shader em Vulkan e OpenGL, com adaptação da convenção de coordenadas.

Framebuffer offscreen na resolução física selecionada: 1280×720, 1920×1080, 2560×1440, 3840×2160 ou 7680×4320. Não depende do tamanho/escala do monitor. Software renderer, GPU ambígua, falta de timestamp, falta de memória ou dimensão acima do limite invalidam o resultado.

Dois segundos de aquecimento, seguidos de 30 ou 60 s medidos divididos em três etapas. GPU medida por timestamps Vulkan ou GL_TIME_ELAPSED. Worker isolado, sem operação para escrever em configurações de hardware. Amostra temperatura periodicamente e interrompe em GPU ≥78 °C ou hotspot ≥95 °C quando esses sensores existem. Um watchdog da interface encerra processos sem progresso.

Para cada cena: `MPix/s = pixels_por_quadro × quadros_por_segundo_GPU / 1.000.000`. A pontuação é a média geométrica desse throughput nas três cenas. É uma escala própria com unidade definida, não uma estimativa de FPS em jogos, nem conversão para pontos 3DMark. Compare mesma API, resolução, método e duração. As cargas das versões 0.1 e 0.2 são diferentes e seus scores não são comparáveis.

`presentedFps` preserva o nome de campo do protocolo anterior, mas nesta versão indica throughput de quadros concluídos pelo worker offscreen, sem apresentação/VSync. O tempo de parede inclui CPU, sincronização e coleta de telemetria. `onePercentLow` usa a média dos 1% maiores tempos desse ciclo, não frametimes de um jogo. `gpuP99Ms` é percentil 99 dos tempos GPU. GPU ms excluem leitura de sensores e gravação da imagem final.

A imagem final é reduzida apenas para visualização; a carga é calculada no tamanho físico informado. O processo verifica identidade PCI quando disponível. OpenGL pode usar correspondência de renderer/modelo único se o driver não expõe fdinfo PCI; duas placas idênticas sem identificação inequívoca são recusadas.

A prévia ao vivo usa uma segunda renderização de 640×360, na mesma GPU, com o mesmo shader, cena e tempo de câmera. É publicada como JPEG atômico para a interface aproximadamente 12–15 vezes/s; não é vídeo previamente gravado. Os timestamps da carga principal não incluem a cópia/JPEG, mas a prévia consome recursos e pode afetar clocks/throughput. `livePreview` registra sua presença. Compare apenas resultados com o mesmo modo de prévia. A resolução medida continua sendo a escolhida, até 8K. CLI de verificação pode usar durações menores identificadas no resultado.

## Computação: `compute-micro-v1`

Kernels Vulkan próprios, buffers de 32 MiB e oito repetições medidas por kernel após uma execução de aquecimento. Leitura reduz valores por workgroup; escrita gera valores; cópia contabiliza tráfego de leitura + escrita. Os valores representam throughput efetivo do kernel, incluindo suas operações auxiliares, não o pico teórico de memória. Cache e boost podem influenciar o resultado.

FP32/FP64: quatro acumuladores FMA, 1024 iterações e 262144 invocações; cada FMA conta como duas operações. Inteiros: quatro sequências multiply/add, mesma contagem de iterações/invocações. As unidades inteiras contam operações lógicas do kernel, não instruções nativas; uma operação de 64 bits pode ser implementada por várias instruções.

Há validação de amostras de saída: exata para memória e inteiros, finitude/faixa para ponto flutuante. Isso detecta falhas grosseiras, não substitui um teste completo de integridade da VRAM ou certificação numérica. FP64 e inteiros de 64 bits são omitidos com estado não suportado quando faltam features. AES, SHA e fractais da imagem de referência não são medidos. Sem score único que misture GB/s, GFLOPS e GIOPS.

## Vídeo: `codec-suite-v0.2`

H.264 encode/decode → HEVC encode/decode → AV1 encode/decode. Cada etapa mede 360 quadros 1080p/8-bit, originados de 120 quadros repetidos três vezes. Prepara a origem fora da medição; limita a preparação CPU a quatro threads. Encoders de preparação: libx264/libx265/libsvtav1.

Encode: leitura da origem, conversão NV12/upload, encoder NVENC ou VA-API a 12 Mbit/s, escrita. Decode: aceleração CUDA/VA-API explícita, saída de hardware e hwdownload obrigatório; frames de software não atendem ao filtro. Throughput inclui inicialização do FFmpeg, I/O, cópias e conversões da fase medida. Não é o pico isolado do engine de vídeo e não compara qualidade equivalente entre encoders/presets distintos.

FPS = quadros efetivamente reportados / tempo monotônico. Exige retorno zero e exatamente 360 quadros. A pontuação é a média geométrica de MPix/s das etapas concluídas, com cobertura explícita. Codec incompatível é identificado; não vira zero fictício. Uma falha genérica de execução invalida o score consolidado. Não compare cobertura parcial com cobertura completa. O JSON registra versão do FFmpeg e argumentos apenas no histórico local; argumentos/caminhos são removidos do feedback.

## Gargalo: `resolution-cpu-transform-v2`

Mesma cena Vulkan em 720p e 1440p, com 150 mil transformações/trigonometria CPU por quadro. Compara tempo GPU e tempo do trabalho CPU, além da razão entre tempos GPU das resoluções. Uma diferença superior a 1,5× indica predominância de uma das cargas; caso contrário, carga mista.

É um diagnóstico sintético limitado, com trabalho CPU de uma thread. Não modela automaticamente motores comerciais, todas as threads, RAM, armazenamento ou PCIe. Não produz um percentual universal de gargalo do computador. A relação de tempos GPU ao mudar resolução também não equivale diretamente ao ganho de FPS de um jogo.


## Gargalo 0.4: resolução alvo e percentuais

Método `resolution-cpu-balance-v3`: referência em 720p e alvo selecionável até 8K, com o mesmo trabalho CPU de uma thread. Para tempos médios C (CPU sintética) e G (GPU), o modelo de sobreposição ideal usa F=max(C,G). Limite de CPU = 100×(1−G/F), relativo ao potencial da GPU; limite de GPU = 100×(1−C/F), relativo ao potencial da CPU. A classificação indica predominância acima de 15%, ou equilíbrio.

Os percentuais são estimativas condicionadas a essa carga/modelo, não uma medição da perda de FPS em jogos. O worker executa trabalho CPU e GPU com sincronização, e a prévia custa recursos; o modelo ideal não representa exatamente seu tempo total. Tempos originais e resolução são preservados no JSON. Registros antigos continuam visíveis sem recalcular seus resultados.
