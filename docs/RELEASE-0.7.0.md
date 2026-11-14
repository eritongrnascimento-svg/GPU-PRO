# GPU-PRO 0.7.0

- Overclock manual sem presets; power limit e fans NVIDIA/AMD quando expostos pelo driver; offset independente de tensão/undervolt AMD compatível.
- FPS atual, 1% low das amostras CSV, média e máxima selecionáveis no GamerMonitor e HUD; gráfico de frametime e expiração de dados antigos.
- Transparência corrigida nos ícones do aplicativo e pacotes; ícone de janela sem halo de tema.
- DEB mantém Baixar no GitHub. AppImage e portáteis preservam atualização gerenciada.
- Leitura real de GPUs NVIDIA via NVML no WSL; adaptador virtual vgem excluído. WSL opera sem ajustes da GPU.

## Pacotes Linux

Linux x86_64, compilação em Ubuntu 24.04. AppImage requer glibc 2.39 ou posterior. TAR/RUN/DEB usam bibliotecas do sistema; consulte as dependências e a documentação de instalação. Nenhum driver de fabricante é incluído. Binários de testes no Ubuntu 26.04 WSL não são usados nesta release.

SHA256SUMS.txt acompanha os pacotes. O AppImage inclui manifesto e fontes correspondentes das dependências. As capturas existentes da versão 0.6.0 foram mantidas; não representam imagens novas da 0.7.0.

## Validação e limites

Compilação e testes automatizados em Ubuntu 24.04; validação adicional de 12 testes no Ubuntu 26.04 WSL, incluindo NVML somente leitura na RTX 3060 Ti. O teste WSL é pulado quando esse ambiente/GPU não está disponível.

Controles dependem da GPU/driver. Não há tensão absoluta NVIDIA nem escrita desses novos controles Intel nesta revisão. Campos não expostos permanecem N/D. O 1% low usa até 240 amostras do CSV, exige 100 amostras e não equivale a estatística por quadro quando o produtor grava por intervalo.

Leia o aviso de riscos antes de ajustes manuais. Testes automatizados e uma GPU não certificam todas as placas, distribuições ou estabilidade de overclock.
