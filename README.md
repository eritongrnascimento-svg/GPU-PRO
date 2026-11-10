# GPU-PRO — 0.4.0

**Titular: Ériton do Nascimento** · [Instalação](docs/INSTALACAO.md) · [Contribuir](CONTRIBUTING.md) · [Licença source available](LICENSE)

Repositório oficial: https://github.com/eritongrnascimento-svg/GPU-PRO

Versão inicial para validação da comunidade; AMD/Intel ainda precisam de testes em hardware. Veja os [resultados reais e limites](docs/VERIFICACAO.md).

Painel MultiGPU Linux em C++20 / Qt 6, com backends NVIDIA, AMD e Intel. O Python original permanece intacto. Esta versão implementa coleta local de resultados para testes entre amigos; não contém servidor, upload automático ou rede neural treinada.

## Iniciar

```sh
./run.sh
```

Abertura: leitura de sensores apenas. Nada de overclock, tensão, power limit, testes ou HUD é aplicado automaticamente. **Esta versão não possui operação para alterar tensão ou limite de potência**, inclusive no helper privilegiado. A potência continua disponível como leitura.

Prévia offline, sem inicializar a GPU:

```sh
./run.sh --preview-offline
./run.sh --preview-offline --preview-page 2
```

Os binários foram compilados neste ambiente Linux x86_64. Não são um AppImage universal: dependem da ABI e bibliotecas do sistema. Para outra distribuição/versão de Qt ou glibc, prefira recompilar com `build.sh`. Não distribua o driver NVIDIA nem bibliotecas gráficas de um fabricante para máquinas de outro.

## Organização da interface

- **GPU**, primeiro menu: Monitor de recursos e Especificações, com cards neon animados.
- **GamerMonitor**: controles de processo/overlay e métricas com rolagem de página.
- **Benchmark**: abas 3D/Jogos, Computação, Encoder/Decoder e Gargalo.
- **Atualizações**: consulta manual às releases oficiais, backup anterior à instalação e restauração.

A cor das bordas acompanha faixas das leituras. Clocks e RPM altos não significam, por si só, falha. Sensores ausentes ficam N/D. O hotspot só é exibido quando o driver fornece uma leitura identificada; não é temperatura média mais um valor calculado. Memória junction não é confundida com hotspot.

O novo teste 3D dura **30 ou 60 segundos**, mais 2 s de aquecimento, e exibe a cena ao vivo. São três cenas procedurais próprias, com câmera, iluminação, sombras e reflexos por shader. Não é conteúdo nem pontuação do AnTuTu. Veja [o método](docs/BENCHMARKS.md).

## Recursos

- **GamerMonitor** substitui o monitor individual.
- A página exclusiva MangoHud foi removida. A integração por perfil continua opcional para métricas/limite de FPS. FSR, DLSS, geração de quadros e upscale não dependem do MangoHud; dependem dos seus respectivos jogos, drivers e integrações.
- **Superior centralizado** no overlay, além dos cantos, centro e coordenadas; X controla deslocamento horizontal e Y a margem superior nessa âncora.
- **GPU/3D:** Vulkan e OpenGL, renderização offscreen real em 720p, 1080p, 1440p, 4K ou 8K, sem precisar de monitor dessa resolução. Verifica a GPU utilizada, limites de imagem e alocação. Software rendering não recebe pontuação válida.
- **Computação:** subaba dentro de GPU/3D, com kernels Vulkan para leitura, escrita, cópia, FP32, FP64, inteiros de 32 e 64 bits; features indisponíveis são identificadas. Valores em GB/s, GFLOPS e GIOPS, sem misturar unidades em um score único.
- **Encoder + Decoder:** uma suíte de seis etapas, H.264 encode/decode, HEVC encode/decode e AV1 encode/decode, com cobertura explícita. Codec incompatível não se transforma em zero fictício; falha de execução impede score consolidado.
- **Históricos:** banco SQLite local, três testes recentes por GPU/categoria, seleção para reabrir o resultado e sua imagem. Os demais registros continuam no banco para feedback.
- **Exportação:** PNG, JPEG e JSON do teste selecionado.
- **Gargalo:** comparação Vulkan 720p/1440p com trabalho CPU fixo, medindo tempos e sensibilidade à resolução. É uma estimativa da carga sintética, sem percentual universal de gargalo de jogos.
- **Sensores:** temperatura NVML atualizada, RPM e porcentagem por ventoinha quando expostos; descoberta de canais hwmon, incluindo junction/hotspot e memória. O driver pode não expor determinados sensores. `0 RPM` e `N/D` têm significados diferentes.
- **Especificações:** ficha técnica organizada com arquitetura, memória, barramento, unidades reportadas, clocks, potência somente leitura, PCIe, driver, VBIOS, Vulkan e resultados de vídeo já testados.
- **Clocks:** barras deslizantes e valores legíveis; candidato NVIDIA de +15 MHz para teste curto com restauração. AMD/Intel mantêm controles manuais somente quando o driver expõe capacidades reconhecidas.

## Feedback dos amigos

1. Instalar/recompilar o aplicativo no ambiente de cada pessoa.
2. Executar os benchmarks desejados em configuração padrão inicialmente.
3. Abrir **Feedback & Diagnóstico → Gerar feedback**.
4. Revisar o JSON mostrado e salvar o arquivo.
5. Enviar o arquivo manualmente ao responsável pelo projeto.
6. No computador do responsável, usar **Importar feedback de amigos**.

O feedback inclui resultados, versão do método, configurações, identificação técnica da GPU, driver, VBIOS e telemetria dos testes. Exclui UUID/serial, hostname, usuário, nomes de processos, caminhos de jogos, argumentos e logs brutos. A exportação é limitada a 16 MiB / 2000 registros recentes e informa quando precisa truncar. Os registros antigos permanecem no banco.

Importações são validadas, sanitizadas, deduplicadas por UUID do **teste** e armazenadas como **não verificadas**. Não são promovidas a histórico local validado nem aplicadas como overclock. Exportar feedback não retransmite automaticamente os dados importados de outras pessoas.

Banco: `~/.config/LinuxGPUControl/LinuxGPUControl/results.sqlite` ou equivalente XDG. Imagens de cenas ficam em `images/` ao lado. Os JSONs da versão 0.1 permanecem no disco; os scores antigos não são recalculados nem equiparados ao método novo.

A base pode alimentar um futuro serviço ou modelo estatístico, mas uma placa não comprova estabilidade de outra. Um teste concluído comprova apenas a execução daquele protocolo, naquele ambiente.

## Clocks: limites e autorização

O helper agora se chama **gpu-clock-helper**. A versão antiga `gpu-power-helper` não faz parte deste pacote. Somente o helper é executado com autorização administrativa; nunca execute a interface inteira como root.

Para instalar o helper após revisar o código:

```sh
sudo install -d -o root -g root -m 0755 /usr/local/libexec/linux-gpu-control
sudo install -o root -g root -m 0755 bin/gpu-clock-helper /usr/local/libexec/linux-gpu-control/gpu-clock-helper
```

Para compilação local, use `build/gpu-clock-helper`. Nenhuma regra polkit permissiva, sudoers ou setuid é criada. A interface verifica propriedade e permissões do executável e dos seus diretórios, e usa o diálogo do `pkexec` do sistema.

- NVIDIA: barras são offsets em MHz.
- AMD moderna: barras são tetos absolutos de frequência OverDrive. Tabelas antigas que exigem tensão são recusadas. Não habilita OverDrive no kernel automaticamente.
- Intel i915: teto nominal de frequência quando exposto; não equivale a overclock irrestrito. Xe pode não expor controle reconhecido nesta versão.
- Aplicação manual permanece no driver até restauração pelo usuário/reinicialização; carregar um candidato nas barras não o aplica.
- Teste automático NVIDIA: baseline Vulkan 720p, incremento **máximo de +15 MHz no core**, segunda carga curta e restauração. Não altera memória, tensão, power limit ou ventoinhas. Exige telemetria válida, temperatura inicial abaixo de 60 °C e GPU pouco ocupada.
- Watchdog no processo privilegiado: leitura a cada ~250 ms, parada a 75 °C / hotspot 95 °C, restauração ao receber STOP, fechar comunicação, perder o proprietário ou atingir 20 s após aplicar. Não há garantia de recuperação após falha de kernel, desligamento ou SIGKILL do próprio watchdog.

Esse teste é curto e não certifica estabilidade prolongada. Um perfil local só é marcado como resultado de teste curto, nunca como homologado pelo fabricante. Referências de outras GPUs não são aplicadas automaticamente. Ainda não há busca automática agressiva, ajuste de tensão ou curva de ventoinha.

## Compilar

Dependências: CMake 3.22+, compilador C++20, pkg-config, Qt 6 Core/Gui/Widgets/Network/OpenGL, SQLite3 (API C, sem plugin Qt SQL), libarchive, GTK 3/GDK, Cairo/Pango, X11, Vulkan-Headers/loader e glslangValidator. Para executar codecs: FFmpeg com NVENC/CUDA ou VA-API conforme GPU/driver. Bibliotecas e render nodes do driver precisam estar acessíveis ao usuário.

```sh
./build.sh
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/linux-gpu-control --ui-smoke
```

Headers Vulkan fora do sistema: `VULKAN_HEADERS_DIR=/caminho/Vulkan-Headers/include ./build.sh`.

Os shaders são compilados e incorporados ao worker; glslang não é dependência de execução. A biblioteca NVML continua opcional em runtime, e sua ausência não impede o uso das outras GPUs. O banco usa SQLite diretamente para não depender de um plugin `QSQLITE` adicional.

## Ver um resultado sem acessar a GPU

```sh
./run.sh --view-report teste.json --preview-page 5
./run.sh --view-report teste.json --export-image teste.png --ui-smoke
./run.sh --view-report teste.json --export-feedback feedback.json --ui-smoke
```

Esses comandos usam um banco em memória e não modificam o histórico principal. Para uma medição computacional: `--preview-page 5 --preview-subtab 1`. Para vídeo: `--preview-page 6`.

## Documentação

- [Métodos e comparabilidade](docs/BENCHMARKS.md)
- [Protocolo de feedback](docs/FEEDBACK.md)
- [Arquitetura](docs/ARQUITETURA.md)
- [Validação executada](docs/VERIFICACAO.md)
- [Roteiro para outras GPUs](docs/VALIDACAO.md)

Na máquina de validação, a instalação do helper foi autorizada e concluída durante os testes. Isso não instala o helper automaticamente nos computadores dos amigos. O teste completo pelo painel registrou o candidato curto local e o restaurou; não há overclock ativo como resultado dessa validação.

Verificação assistida para desenvolvimento (abre autenticação do sistema e executa o teste curto; não é apenas uma prévia):

```sh
./run.sh --clock-check resultado-clock.json
```

Essa opção usa o mesmo fluxo do botão, incluindo baseline, espera de ociosidade/temperatura, confirmação do offset durante a renderização e restauração. A inicialização normal, sem essa opção, não executa testes.


## Licença e publicação

Código disponível sob [GPU-PRO Source Available License 1.0](LICENSE), com forks no GitHub para auditoria/contribuição e restrições de redistribuição. Não é uma licença open source da OSI. O mantenedor decide quais PRs e releases são oficiais; dependências mantêm suas próprias licenças em [NOTICE](NOTICE).

Consulte a [revisão de compatibilidade das licenças](docs/LICENCAS.md), [Instalação](docs/INSTALACAO.md), [Publicação](docs/PUBLICACAO.md), [Changelog](CHANGELOG.md) e [Segurança](SECURITY.md).


## Novidades da versão 0.4

A pasta do projeto passa a se chamar **GPU-PRO**, preservando históricos e configurações no diretório anterior de dados do usuário. Nenhum teste ou perfil antigo é apagado.

- Especificações ajustam colunas e alturas dos cards à janela; janelas pequenas mantêm rolagem para preservar a legibilidade.
- Gargalo aceita 720p, 1080p, 1440p, 4K e 8K como resolução alvo, com referência 720p e resumo. Os percentuais estimam o limite CPU/GPU na carga sintética com sobreposição ideal; não são perda real medida de FPS em jogos.
- Atualizações inclui a opção de fechar para a bandeja e gerenciamento de drivers por APT, pacman, DNF e backend PackageKit da distribuição, com consulta de versões, simulação e confirmação no gerenciador nativo. Não adiciona repositórios externos ou instala automaticamente. Em Arch/CachyOS usa atualização completa do sistema e versão explícita.
- Novos formatos: `.deb` para Ubuntu 24.04 amd64, AppImage x86_64 e executável portátil `.run`. O `.run` inclui os workers, não exige root e utiliza bibliotecas do sistema. AppImage tem base Ubuntu 24.04/glibc 2.39 ou posterior; não é universal para distribuições antigas. Consulte as fontes correspondentes das dependências incluídas no AppImage, fornecidas junto da release.

AppImage e .deb consultam versões pelo painel e abrem o download oficial correspondente; a substituição desses formatos usa o instalador/arquivo original, após fechar o aplicativo. O atualizador com backup automático da pasta permanece disponível na edição portátil `.tar.gz`/`.run`.

A detecção utiliza o gerenciador e os repositórios reais do sistema; PackageKit fornece um backend comum para outras distribuições quando instalado e funcional. NixOS e sistemas imutáveis são reconhecidos como declarativos/deployments e não recebem comandos de instalação imperativa de outra distro. Não existe garantia de instalação automática para qualquer distribuição Linux sem um backend compatível.
