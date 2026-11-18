# GPU-PRO 0.8.1

Versão de correção da 0.8.0, para Linux e Windows. A prioridade foi o FPS dos jogos no HUD, que não aparecia sem ferramentas externas.

## FPS dos jogos sem programas externos

- **Windows:** o GPU-PRO conta os quadros sozinho, sem PresentMon. Lê do próprio Windows cada quadro apresentado em DirectX 9–12, Vulkan e OpenGL, sem tocar no jogo. O Windows só libera essa leitura a administradores, por isso o contador roda como um serviço pequeno ("GPU-PRO · Contador de quadros"), instalado **uma única vez** pelo instalador ou pelo botão **Ativar FPS automático** em GamerMonitor & Overlay. Ele só lê eventos enquanto o GPU-PRO está aberto e pode ser removido pelo próprio app. Há também a opção **Só nesta sessão**, com uma confirmação do Windows a cada abertura.
- **Linux:** o GPU-PRO traz a própria camada Vulkan, sem MangoHud. Ativada uma vez (botão **Ativar FPS automático**), ela conta os quadros de jogos Vulkan nativos e de todos os jogos pelo Proton (DirectX 9–11 via DXVK e DirectX 12 via VKD3D-Proton). Jogos OpenGL nativos usam o lançador `gpu-pro-fps %command%` nas opções de inicialização do Steam.
- **Jogo detectado automaticamente:** o jogo em primeiro plano; fora dele, o último jogo detectado; compositor, gravação de tela, navegadores e sobreposições são ignorados. A lista de processos continua para escolher outro, e um CSV escolhido manualmente continua tendo prioridade.
- Validação: no Windows, janelas de teste DirectX 11, OpenGL e Vulkan a 60 Hz foram medidas a 60,0, 60,2 e 59,9 FPS lidas sem administrador; no Linux (WSL), Vulkan pela camada e OpenGL pelo lançador.

## Benchmark

- **Teste completo:** todos os testes em sequência, como um benchmark completo, com pontuação total e pontuação de cada categoria (GPU 3D, memória da GPU, computação da GPU, codecs de vídeo, codecs de imagem e processador). Cada teste continua disponível sozinho.
- **Cenas 3D:** resultado em linguagem simples — método resumido, pontuação 3D e FPS mínimo, médio e máximo (por segundo de teste), além do 1% low.
- **Memória e computação:** pontuação de memória (leitura, escrita e cópia na VRAM, envio CPU → GPU e retorno GPU → CPU pelo PCIe, cópia direta na VRAM) e pontuação de computação (FP32, FP64 e inteiros).
- **Codecs · vídeo e imagem:** H.264, HEVC e AV1 por hardware, codificação e decodificação em 720p, 1080p, 1440p e 4K, com FPS, megapixels por segundo e pontuação; imagens JPEG, PNG e WebP (quando disponível) nas mesmas resoluções, com pontuação própria.

## Outras mudanças

- **Notificações & Logs:** ao buscar no log, cada trecho encontrado ganha o botão **Abrir trecho**, que mostra o registro completo (resultados de teste formatados) com as linhas ao redor e permite copiá-lo.
- **Feedback & Diagnóstico:** removidos os botões "Importar feedback de amigos" e "Consultar banco importado".
- PresentMon saiu da lista de dependências do Windows.

## Pacotes

**Windows 10/11 x64:** `GPU-PRO-0.8.1-windows-x64-setup.exe` e `GPU-PRO-0.8.1-windows-x64.zip`, com `SHA256SUMS-windows-0.8.1.txt`. **Linux x86_64** (Ubuntu 24.04): DEB, AppImage, RUN e TAR, com código-fonte, manifesto e fontes das dependências (`SHA256SUMS.txt`). A camada de FPS do Linux é de 64 bits; jogos de 32 bits ainda não são contados.

## Limites

As pontuações são escalas próprias do GPU-PRO e não equivalem a pontos de outros benchmarks; compare mesma versão de método. A camada do Linux no Steam em Flatpak e jogos com anti-cheat ainda precisam de testes em mais jogos. Leia o [aviso de responsabilidade](AVISO-DE-RESPONSABILIDADE.md) antes de qualquer ajuste manual.
