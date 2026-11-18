# GPU-PRO para Windows

Versão 0.8.1 · Windows 10 1809 ou mais novo, x64. A interface é WinUI 3. Os testes e leituras
usam o mesmo núcleo C++ da versão Linux.

## Instalação

**Instalador (recomendado):** execute `GPU-PRO-0.8.1-windows-x64-setup.exe`. Ele instala para o
usuário atual, sem precisar de administrador; na primeira tela também é possível instalar para
todos os usuários. O instalador cria atalhos no menu Iniciar (e, se você quiser, na área de
trabalho) e registra o GPU-PRO em Aplicativos instalados, por onde ele pode ser desinstalado.

**Portátil:** extraia `GPU-PRO-0.8.1-windows-x64.zip` em uma pasta sua e abra `GPU-PRO.exe`.

.NET, Windows App SDK, Qt (só para os executores de teste) e o runtime C++ do Visual Studio já
vêm junto. Driver da GPU e Vulkan vêm do fabricante. FFmpeg (testes de vídeo) pode ser instalado em
**Atualizações › Instalar dependências** (winget).

## FPS dos jogos

O GPU-PRO conta os quadros sozinho, sem PresentMon: lê do próprio Windows (ETW) cada quadro
apresentado em DirectX 9–12, Vulkan e OpenGL, sem tocar no jogo, e detecta o jogo automaticamente
(primeiro plano; fora dele, o último jogo detectado). O Windows só libera essa leitura a
administradores, então o contador `gpupro-frames.exe` roda como o serviço
**GPU-PRO · Contador de quadros**, instalado uma vez — pela opção do instalador ou pelo botão
**Ativar FPS automático** em GamerMonitor & Overlay. O serviço fica em
`%ProgramFiles%\GPU-PRO\FrameService` (pasta que só administradores alteram), só lê eventos enquanto
o GPU-PRO está aberto, não aceita comandos e pode ser removido pelo botão **Remover contador** ou ao
desinstalar. Sem o serviço, **Só nesta sessão** abre o contador com uma confirmação do Windows; se o
GPU-PRO já roda como administrador, o contador abre junto sem perguntar. Um CSV de frametime
escolhido manualmente continua tendo prioridade.

Configurações, histórico de testes e logs ficam em `%LOCALAPPDATA%\GPU-PRO` e são mantidos ao
desinstalar.

Ao abrir **Overclock**, o app exibe o termo de responsabilidade e assunção de riscos
(`docs/AVISO-DE-RESPONSABILIDADE.md`). A aba só abre depois do aceite. O app sempre abre
centralizado na tela principal.

## Abas

| Aba | O que faz |
| --- | --- |
| GPU · Monitor e especificações | Sensores ao vivo em cards que se ajustam à janela. Ficha técnica igual à do Linux: DXGI, NVML e Vulkan (associado à GPU pelo LUID). |
| CPU · Monitor e cálculos | Uso por processador lógico e testes sintéticos. |
| Configurações 3D & Filtros | Recursos que a GPU suporta (DXR, DX12 Ultimate, mesh shaders, VRS, DLSS/FSR/XeSS/NIS). Controles do driver NVIDIA (perfil global). Opções do Windows: Auto HDR, VRR, jogos em janela, Modo de Jogo e agendamento de GPU. GPU por jogo. |
| GamerMonitor & Overlay | HUD configurável, com FPS e frametime, GPU, CPU, memória e disco. Aparece sobre o jogo ou na área de trabalho, com formato, cores, tamanho, posição e monitor ajustáveis. |
| Monitores & Telas | Resolução, frequência e orientação, com confirmação de 15 s. Também escala, HDR, monitor principal, brilho/contraste (DDC/CI) e perfil de cor. |
| Overclock · Manual | Faixas reconhecidas pelo driver. Cada ajuste pede confirmação e autorização do Windows. Gráfico de estabilidade dos últimos 10 minutos (clock, memória, temperatura, potência, ventoinha e uso), com marcas dos ajustes, diagnóstico sob carga e exportação CSV. |
| Benchmark | Cenas 3D, memória/FLOPS, encoder/decoder e gargalo. O gargalo informa quanto GPU, CPU e memória perdem nesta carga. |
| Notificações & Logs | Único lugar com mensagens do app: notificações e log de operações, com busca e filtro por categoria. |
| Sobre | O projeto, links do repositório, o "cafezinho" por PIX (QR e copia e cola) e um versículo sorteado, que também aparece na barra lateral. |

## O que pede administrador

- Gravar configurações 3D no driver NVIDIA (`gpu-3d-worker.exe`).
- Ajustes de overclock (`gpu-tuning-worker.exe`).
- Agendamento de GPU acelerado por hardware. Só vale depois de reiniciar.

Em todos os casos o Windows mostra o pedido de autorização (UAC). Nada é aplicado ao abrir o app.

## Limites conhecidos

- FSR, XeSS e DLSS sem override são ativados dentro de cada jogo.
- Controles de driver AMD e Intel ainda não fazem parte desta versão. Os recursos suportados
  aparecem na aba 3D.
- Tela cheia exclusiva pode cobrir qualquer janela, inclusive o HUD. Use tela cheia sem bordas.
- A temperatura da CPU só aparece quando o Windows expõe o sensor às APIs de usuário.
- Brilho e contraste dependem de DDC/CI habilitado no monitor.

## Compilar

```powershell
./windows/build-windows.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64 -Glslang C:\glslang\bin\glslang.exe `
    -VulkanInclude C:\Vulkan-Headers\include -VulkanLibrary C:\vulkan-1.lib
```

Requisitos: Visual Studio 2022 (C++), .NET SDK 8, CMake ≥ 3.22. O script compila, roda os testes
e gera `dist/GPU-PRO-<versão>-windows-x64.zip` com o SHA-256.
