# Controles manuais de GPU

A página Overclock não carrega presets nem aplica ajustes ao abrir. O aceite por sessão dá acesso à página; cada aplicação continua exigindo confirmação e autenticação pkexec. A interface inteira nunca precisa ser executada como root. Atualize/reinstale o helper compilado junto com esta revisão; helpers antigos recusam os novos comandos.

| Controle | NVIDIA | AMD amdgpu | Intel |
|---|---|---|---|
| Core/memória | Offsets NVML reconhecidos | Tetos OverDrive reconhecidos | Teto i915 já implementado, quando exposto |
| Power limit | Limite NVML em W | power1_cap hwmon em W | Não implementado nesta revisão |
| Tensão / undervolt | Não exposto por este backend | Offset independente OD_VDDGFX_OFFSET em mV | Não implementado nesta revisão |
| Ventoinhas | Por índice NVML, percentual e automático | PWM hwmon, percentual e automático | Não implementado nesta revisão |

Consumo (W) é a leitura medida. Power limit (W) é um teto configurado, não uma promessa de consumo fixo. Tensão GPU (mV) é a leitura de in0_input reconhecida no amdgpu. Undervolt reduz a tensão por offset negativo; não é um segundo tipo de consumo. Não se oferece um controle fictício de tensão absoluta quando a interface do driver não existe. Em APUs AMD, potência hwmon pode incluir a CPU.

O helper consulta novamente as faixas do driver e recusa valores externos. Não habilita OverDrive pelo kernel nem modifica BIOS. Ajustes de fan manual conferem política/target ou PWM; RPM físico pode demorar a acompanhar. Use **Restaurar controle automático das ventoinhas** para devolver o gerenciamento ao driver. A restauração de potência/tensão usa os valores iniciais disponíveis nesta sessão (potência em passos de 1 W).

As escritas são confirmadas por leitura; falhas provocam tentativa de retorno ao limite/offset anterior ou modo automático de fan, informando quando a restauração não pôde ser confirmada. Ajustes manuais permanecem até restauração ou reinicialização e não possuem o watchdog do antigo diagnóstico curto. Não há aplicação automática de perfis nem curva de fan nesta revisão.

## Referências das interfaces

- [NVML: comandos de dispositivo](https://docs.nvidia.com/deploy/nvml-api/620/api/group__nvmlDeviceCommands.html)
- [Kernel: controles de potência/térmicos do amdgpu](https://www.kernel.org/doc/html/latest/gpu/amdgpu/thermal.html)

## Testes no Windows com WSL

Instale as dependências Linux descritas no README, incluindo qt6-wayland e fonts-noto-core no Ubuntu. Compile no diretório Linux com build.sh e execute o binário pelo WSL. A opção --preview-offline exibe dados fictícios sem acessar GPU; --start-maximized abre a janela maximizada. Para Wayland no WSLg, defina QT_QPA_PLATFORM=wayland apenas no processo do aplicativo.

Binários compilados em Ubuntu 26.04 para testes locais não substituem os pacotes oficiais recompilados em Ubuntu 24.04. Os lançadores usados em um computador devem apontar para o caminho Linux do build instalado nele.

## Correção de sensores no WSL

No WSL, GPUs NVIDIA também são enumeradas diretamente por NVML. A placa virtual
vgem não é apresentada como a GPU física. Essa descoberta é somente leitura;
o helper recusa escritas no WSL. No Linux nativo a enumeração e os controles
existentes são preservados.

O lançador Windows usa Ubuntu-26.04, Wayland e janela maximizada. A família
fonts-noto-core fornece as fontes usadas pelo tema. Não instale um driver NVIDIA
Linux para substituir a biblioteca fornecida pelo driver Windows no WSL.

A validação real em RTX 3060 Ti confirmou sensores, 8 GiB de VRAM, arquitetura
Ampere, barramento de 256 bits, clocks, PCIe, driver e VBIOS. Campos que a NVML
do WSL não fornece continuam N/D. A captura somente de leitura pode ser feita
com a opção --capture-live seguida do caminho PNG; use --preview-subtab 1 para
a ficha técnica. Ela não executa benchmarks ou alterações de GPU.

Referência: https://docs.nvidia.com/cuda/wsl-user-guide/
