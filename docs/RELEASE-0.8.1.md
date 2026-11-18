# GPU-PRO 0.8.1 · Patch de correção

Patch da versão 0.8.0 para Linux e Windows, publicado em 18/11/2026. Corrige o FPS dos jogos no HUD e deixa os resultados do Benchmark mais claros. Não muda configurações salvas, perfis nem o histórico de testes: basta instalar por cima da 0.8.0.

## O que este patch corrige

| Problema na 0.8.0 | Correção na 0.8.1 |
|---|---|
| **Windows:** o HUD não mostrava o FPS do jogo. A captura dependia do PresentMon, precisava ser iniciada à mão escolhendo o processo, e o Windows recusava a leitura sem administrador, sem mostrar erro. | O GPU-PRO conta os quadros sozinho, pelos eventos do próprio Windows, sem PresentMon: DirectX 9–12, Vulkan e OpenGL, sem tocar no jogo. O contador é um serviço pequeno ("GPU-PRO · Contador de quadros") instalado **uma única vez** pelo instalador ou pelo botão **Ativar FPS automático**; depois o FPS aparece sozinho. |
| **Linux:** o FPS só aparecia com um CSV do MangoHud escolhido manualmente, e o perfil de jogo não ligava a gravação desse CSV. | O GPU-PRO traz a própria camada Vulkan, sem MangoHud. Ativada uma vez, conta os quadros de jogos Vulkan nativos e de todos os jogos pelo Proton (DirectX 9–11 via DXVK, DirectX 12 via VKD3D-Proton). Jogos OpenGL nativos usam `gpu-pro-fps %command%` nas opções do Steam. |
| Era preciso escolher o processo do jogo para medir o FPS. | O jogo é detectado automaticamente: o que está em primeiro plano ou, fora dele, o último jogo detectado. Compositor, gravação de tela, navegadores e sobreposições são ignorados. A lista de processos e o CSV manual continuam disponíveis. |
| O resultado das Cenas 3D mostrava apenas um número em "MPix/s geométrico", difícil de interpretar. | Resultado em linguagem simples: método resumido, **pontuação 3D** e **FPS mínimo, médio e máximo**, além do 1% low. |
| O teste de memória não tinha pontuação nem media transferências entre CPU e GPU. | **Pontuação de memória** (leitura, escrita e cópia na VRAM, envio CPU → GPU e retorno GPU → CPU pelo PCIe, cópia direta na VRAM) e **pontuação de computação** (FP32, FP64 e inteiros). |
| Os codecs eram medidos só em 1080p, e não havia teste de imagens. | Vídeo H.264, HEVC e AV1, codificação e decodificação por hardware em **720p, 1080p, 1440p e 4K**, com FPS, megapixels por segundo e pontuação; **imagens** JPEG, PNG e WebP (quando disponível) nas mesmas resoluções, com pontuação própria. |
| A busca no log só filtrava as linhas; registros longos (resultados de testes) ficavam difíceis de ler. | Cada trecho encontrado ganha o botão **Abrir trecho**, que mostra o registro completo, com os resultados formatados e as linhas ao redor, e permite copiá-lo. |
| **Windows:** o app não usava a cor da GPU em todo lugar. Botões e o nome no topo ficavam sempre verdes, e o indicador do menu, as abas e as caixas de seleção usavam a cor de destaque do Windows. | O app identifica a GPU selecionada ou conectada e usa a cor dela em toda a interface, como no Linux: NVIDIA verde, AMD vermelho, Intel azul. O tema do Windows não é alterado. |
| **Windows:** o Teste completo somava 0 pontos no total e nas categorias. | Soma corrigida; cada categoria mostra a própria pontuação. |
| **Windows:** depois do novo teste de codecs, a aba Especificações ficava toda N/D e o app fechava sozinho (codecs repetidos por resolução na ficha). | Uma linha por codec e modo, com as resoluções suportadas por hardware; a ficha volta a aparecer e o app não fecha. No Linux, a mesma ficha mostrava só a última resolução; o código-fonte também foi corrigido. |
| **Windows:** textos com acento e "×" apareciam embaralhados nos resultados; o botão "Ver três últimos resultados" abria uma lista sem identificar os testes; todos os resultados apareciam num quadro só, abaixo das abas. | Comunicação com os executores em UTF-8 e conserto dos textos já salvos; cada aba tem o próprio quadro de resultado, que aparece assim que o teste termina, e a lista **Teste 1 (mais recente), Teste 2 e Teste 3** com data, hora e pontuação. |
| Feedback & Diagnóstico tinha os botões "Importar feedback de amigos" e "Consultar banco importado", fora do uso atual. | Botões removidos. |

## Novo no Benchmark

**Teste completo:** todos os testes em sequência, como um benchmark completo, com pontuação total e a pontuação de cada categoria (GPU 3D, memória da GPU, computação da GPU, codecs de vídeo, codecs de imagem e processador). Cada teste continua disponível sozinho nas outras abas. Os métodos e as fórmulas das pontuações estão em [Benchmarks](BENCHMARKS.md).

## Validação

- Windows: janelas de teste DirectX 11, OpenGL e Vulkan a 60 Hz lidas pelo contador a 60,0, 60,2 e 59,9 FPS sem administrador no aplicativo; executores de benchmark medidos numa RTX 3060 Ti.
- Linux: camada Vulkan e lançador OpenGL validados no WSL; compilação e 11 testes automáticos no Ubuntu 24.04.
- O titular instalou o patch e confirmou o FPS e o aplicativo funcionando no Windows e no Linux.

## Pacotes

**Windows 10/11 x64:** `GPU-PRO-0.8.1-windows-x64-setup.exe` (marque "Ativar FPS automático dos jogos"; pede administrador uma vez) e `GPU-PRO-0.8.1-windows-x64.zip`, com `SHA256SUMS-windows-0.8.1.txt`.

**Linux x86_64** (Ubuntu 24.04): DEB, AppImage (glibc 2.39 ou posterior), RUN e TAR, com código-fonte, manifesto e fontes das dependências (`SHA256SUMS.txt`). Depois de instalar, clique uma vez em **Ativar FPS automático** em GamerMonitor & Overlay.

## Limites

- A camada de FPS do Linux é de 64 bits; jogos de 32 bits ainda não são contados. Steam em Flatpak e jogos com anti-cheat precisam de testes em mais jogos.
- As pontuações são escalas próprias do GPU-PRO e não equivalem a pontos de outros benchmarks; compare a mesma versão de método.

Leia o [aviso de responsabilidade](AVISO-DE-RESPONSABILIDADE.md) antes de qualquer ajuste manual.
