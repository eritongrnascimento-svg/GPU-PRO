# Drivers, atualização do aplicativo e preferências do sistema

As funções ficam em **Atualizações**. Consultas não instalam drivers. Instalações exigem revisão da prévia, confirmação e autenticação do sistema. Evite transações de pacotes durante gravação, transmissão ou trabalho sem salvar.

## Drivers completos

A primeira lista mostra conjuntos principais disponíveis nos repositórios já configurados, em ordem de versão. A segunda mostra a instalação atual. Ferramentas de contêiner e componentes auxiliares não são oferecidos como se fossem um driver completo. Open/proprietário refere-se à variante do módulo NVIDIA; Open não significa que todos os componentes NVIDIA tenham código aberto.

No Arch/CachyOS, o conjunto relaciona módulo, bibliotecas compatíveis e componentes de 32 bits/OpenCL instalados. Módulos pré-compilados são relacionados aos kernels presentes. DKMS pode precisar de headers e compilação. Linhas legadas evidentemente inadequadas à arquitetura detectada são filtradas por precaução; disponibilidade no repositório não é homologação para toda GPU.

**Simular instalação** apresenta adições, atualizações e remoções. O backend Arch invoca `pacman` como programa externo dentro de `bubblewrap`: sem rede, sistema somente de leitura, estado temporário e resposta negativa na confirmação final. O aplicativo não vincula libalpm. Se o isolamento ou a interpretação completa da tabela não funcionar, a instalação fica bloqueada. O pacote `bubblewrap` e namespaces de usuário precisam estar disponíveis.

A instalação real abre o gerenciador nativo e solicita autenticação. No Arch utiliza atualização completa com os mesmos índices locais da simulação (`-Su`), sem uma sincronização posterior que mude silenciosamente a versão escolhida. Os índices precisam estar válidos; confirme a transação final no terminal. APT/DNF usam suas prévias nativas. PackageKit depende de o backend conseguir representar um conjunto principal reconhecido; não há garantia de instalação automática em toda distribuição.

## Restaurar driver anterior

Antes da troca Arch, o aplicativo registra o estado e tenta preservar os pacotes anteriores encontrados no cache, com seus checksums e assinaturas disponíveis. Apenas conjuntos completos habilitam restauração por arquivos. Se faltarem pacotes, o painel informa que não poderá garantir esse rollback e pede decisão explícita antes de prosseguir.

A restauração passa novamente pelo resolvedor e pelo gerenciador nativo. Módulos e bibliotecas precisam permanecer coerentes com o kernel; drivers Intel/AMD também envolvem componentes do kernel e do Mesa. Dependências novas podem permanecer instaladas. O aplicativo não promete restaurar todo o sistema ou recuperar automaticamente uma sessão que não inicialize.

Nos backends que registram referências de versões, a versão anterior precisa continuar disponível no repositório e passar na simulação; uma referência não é um arquivo de backup nem uma garantia de disponibilidade futura. Atualizar o driver carregado pode exigir reinicialização. Não são usados instaladores `.run` de fabricantes ou repositórios adicionados sem solicitação.

## Atualizar DEB e AppImage pelo painel

**Verificar atualizações** consulta a release oficial. **Baixar, salvar backup e instalar atualização** baixa o formato correspondente e confere seu SHA-256.

- **AppImage:** exige arquivo normal em pasta gravável pelo usuário. Salva uma cópia integral e substitui o arquivo no mesmo caminho; a execução atual termina com a versão carregada anteriormente. Reinicie pelo painel. Restaurar último backup repõe o arquivo anterior.
- **DEB em sistema APT:** valida nome `gpu-pro`, arquitetura e versão do pacote, preserva o DEB oficial da versão atual e mostra a simulação APT. A instalação é feita no gerenciador nativo com autenticação. Se não conseguir preservar a versão anterior ou se a execução não corresponder ao pacote instalado, não prossegue. Reinicie antes de uma nova operação. O backup pode ser reinstalado pelo painel ou manualmente pelo APT.
- **Pacote convertido para outro gerenciador:** não recebe transação APT forçada. Use seu gerenciador ou o AppImage oficial.
- **TAR/RUN:** preservam o atualizador de pasta com backup/restauração existente. Builds locais continuam sendo recompilados.

O botão de abrir o GitHub continua disponível. Checksum da própria release verifica integridade, mas não é uma assinatura independente contra comprometimento da conta do mantenedor. Históricos e configurações de usuário não fazem parte da substituição do pacote.

## GPU preferida

A preferência por aplicativo se aplica a novos programas compatíveis com PRIME lançados pelo painel. Processos existentes não trocam de GPU, e nem toda aplicação respeita essas variáveis. Benchmarks mantêm seleção explícita do dispositivo. A opção Automático devolve a decisão à política do sistema.

A configuração de sessão está disponível para KDE Wayland com ambas as GPUs ativas, para preservar o mapeamento dos dispositivos e conectores. Mostra um arquivo revisável em `~/.config/plasma-workspace/env/gpu-pro-gpu.sh`, com aliases estáveis para DRM e backup. Exige sair e entrar na sessão. Não altera firmware, cabos ou kernel, nem troca a sessão gráfica em funcionamento. Outros compositores não recebem esse arquivo como se fossem KDE.

**Desfazer preferência da sessão** restaura a configuração anterior. Se o arquivo tiver sido alterado externamente, a reversão automática é recusada para preservá-lo. Em uma sessão que não abre, esse arquivo de preferência pode ser removido pelo usuário em um terminal de recuperação antes de iniciar novamente; mantenha acesso a esse caminho e ao backup.

## Alinhar saídas de áudio

O botão consulta o servidor de áudio e mostra entrada e saída disponíveis, distinguindo dispositivos virtuais. Salva os destinos anteriores e aplica a escolha como padrão para novas conexões. Preferências escolhidas ficam disponíveis na próxima abertura do diálogo. **Restaurar destinos anteriores** oferece desfazer.

Não reinicia PipeWire/WirePlumber, não muda volumes, não remove EasyEffects e não move conexões abertas do OBS. Portanto, uma captura já configurada explicitamente pode continuar no dispositivo anterior até ser ajustada no OBS. Ausência de entrada física é mostrada; uma entrada virtual não comprova microfone funcional.

Falhas como `no codecs initialized` acontecem antes do roteamento: o botão não cria uma placa que o ALSA não conseguiu inicializar. Esse caso exige diagnóstico de firmware, kernel e codec. Não há aplicação automática de quirks, blacklist global ou parâmetros genéricos de áudio.
