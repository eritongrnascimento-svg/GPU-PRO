# GPU-PRO 0.8.0

Primeira versão para **Windows** e redesenho do **Linux** para ficar igual a ela, respeitando as funções de cada sistema. Abaixo, tudo o que mudou depois da 0.7.0.

## Novidades (Linux e Windows)

- **Mesmo design nos dois sistemas:** nome GPU-PRO e ícone na barra de título, menu na mesma ordem e janela que sempre abre centralizada (no Wayland nativo, a posição é decidida pelo compositor).
- **Notificações & Logs:** uma única aba com duas sub-abas, Notificações e Log de operações. O log tem busca e filtro por categoria e uma cópia diária em arquivo. Nenhuma outra aba exibe logs.
- **Overclock:** termo de responsabilidade e assunção de riscos (versão 2) ao abrir a aba, com base no CF art. 5º, II, CC arts. 186, 927 e 945, CDC art. 12, § 3º, III, e art. 14, § 3º, II, e Lei 9.609/1998. Gráfico detalhado de estabilidade dos últimos 10 minutos (clock, memória, temperatura, potência, ventoinha e uso), marcas dos ajustes aplicados, diagnóstico sob carga (Estável / Oscilação leve / Instável, com causa provável) e exportação CSV. Controles e cards compactados.
- **Gargalo em linguagem direta:** "GPU perdendo X% de desempenho", "CPU perdendo X%" e "Memória perdendo X%". A memória só perde desempenho quando a VRAM transborda para a RAM do sistema.
- **GamerMonitor & Overlay:** HUD com fundo translúcido e desfoque, no jogo ou na área de trabalho. Escolha de cada leitura: FPS com frametime opcional, GPU, CPU, memória e disco (leitura/escrita, tempo ativo e temperatura). Cores de fonte, título e gráfico, tamanho, posição e formato configuráveis. Página compactada.
- **Configurações 3D & Filtros:** mostra os recursos que a GPU e as bibliotecas instaladas suportam (ray tracing, DLSS/Frame Generation, FSR, XeSS, NIS, nitidez, HDR, VRR) e os ajustes disponíveis em cada sistema.
- **Monitores & Telas:** resolução, frequência, orientação, escala, HDR, monitor principal, brilho/contraste por DDC/CI e perfil de cor dentro do app, com reversão automática em 15 s para mudanças de geometria. Um único botão abre as configurações de tela do sistema.
- **Sobre:** informações do projeto, link do repositório, cafezinho por PIX (QR e copia e cola) e versículo bíblico aleatório, também exibido no rodapé do menu.

## Somente Windows

- Interface WinUI 3 sobre o mesmo núcleo C++ do Linux; leituras por NVML, DXGI, contadores do Windows, D3D12 e Vulkan.
- Especificações da placa no mesmo nível de detalhe do Linux e cards do monitor que se ajustam à janela.
- Configurações 3D do driver NVIDIA no perfil global, Auto HDR, VRR, otimizações para jogos em janela, Modo de Jogo, agendamento de GPU por hardware e GPU por jogo.
- Correção: o app podia fechar sozinho ao trocar de aba (validado com 112 trocas de aba e 16 ciclos do HUD).

## Somente Linux

- Escala, HDR, VRR e perfil de cor pelo KDE (kscreen-doctor); modo, rotação e monitor principal também por xrandr; DDC/CI por ddcutil.
- Perfis por jogo com Gamescope (FSR/NIS, nitidez, HDR, VRR), DLSS via DXVK-NVAPI, ray tracing DX12 e HDR no Proton, GameMode.
- HUD com RAM, clock da CPU e disco (temperatura NVMe/SATA); na AMD, o GTT confirma o transbordo de memória no teste de gargalo.
- Correções: títulos com "&" exibidos como "_"; janela do HUD e exportação de imagem com o nome GPU-PRO.

## Pacotes

**Windows 10/11 x64:** `GPU-PRO-0.8.0-windows-x64-setup.exe` (instala por usuário, sem administrador) e `GPU-PRO-0.8.0-windows-x64.zip` (portátil). Checksums em `SHA256SUMS-windows-0.8.0.txt`. Veja [Windows](WINDOWS.md).

**Linux x86_64**, compilado em Ubuntu 24.04: DEB, AppImage (glibc 2.39 ou posterior), RUN e TAR, com código-fonte, manifesto e fontes das dependências. Checksums em `SHA256SUMS.txt`. Nenhum driver de fabricante é incluído.

Os executáveis Windows não têm assinatura digital de código; o SmartScreen pode pedir confirmação na primeira execução.

## Validação e limites

- Windows: compilação, testes do núcleo, instalador e uso real em uma RTX 3060 Ti (capturas da galeria 0.8.0).
- Linux: compilação e testes em Ubuntu 24.04 e no Ubuntu 26.04 via WSL, com o aplicativo executado no WSLg.
- Ainda não verificado em hardware real: gravação de configurações do driver, overclock aplicado, HDR e mudanças de tela, DDC/CI, HUD do Linux dentro de jogos e GPUs AMD/Intel no Windows. Campos que o driver não expõe aparecem como N/D.

Leia o [aviso de responsabilidade](AVISO-DE-RESPONSABILIDADE.md) antes de qualquer ajuste manual. Testes automatizados e uma GPU não certificam todas as placas, sistemas ou a estabilidade de overclock.
