# Ícones, bandeja e instância única

## Identidade visual — 0.6.1

O menu utiliza a arte fornecida para o GPU-PRO. O arquivo recebido com extensão `.svg` contém uma imagem WebP; a arte original é preservada em `assets/gpu-pro-original.webp`, convertida sem redesenho em PNG e incluída no SVG de empacotamento. Não é anunciada como um desenho vetorial puro.

A janela utiliza essa mesma arte, removendo em memória apenas o fundo preto conectado ao exterior da imagem. Detalhes pretos fechados dentro do desenho são preservados. O contorno neon acompanha a GPU selecionada: verde NVIDIA, vermelho AMD e azul Intel. A mudança é feita na seleção da GPU; o ícone da janela não fica repintando a interface continuamente.

A bandeja utiliza o monograma **GP**, com núcleo legível e halo pulsante suave, aproximadamente cinco quadros por segundo. Os quadros são pré-calculados, sem coleta adicional de sensores e sem piscar entre ligado/desligado. A animação pausa enquanto o menu da bandeja está aberto. **Animar neon do ícone GP**, no menu da bandeja, permite desativá-la; a escolha é salva. Essa preferência é independente da animação dos cards.

O ícone da janela também é disponibilizado ao sistema para a barra de tarefas. O compositor, a decoração da janela e o agrupamento com atalhos fixados podem preferir o ícone estático do arquivo `.desktop`. Não há alteração automática de temas, configuração global de KDE/GNOME ou decoração do sistema.

## Ícone antigo no menu

O arquivo `.desktop` mantém sua identidade `gpu-pro`, mas passa a usar o nome de ícone específico `gpu-pro-brand-v061`, evitando reutilizar a entrada visual antiga ou uma substituição genérica do tema. O DEB atualiza os caches compartilhados de ícones e aplicativos quando essas ferramentas estão disponíveis. A sessão pode precisar reler o menu após a instalação; atalhos fixados antigos podem precisar ser removidos e fixados novamente.

AppImages e instalações portáteis podem depender da integração de atalhos escolhida pelo usuário. O aplicativo não sobrescreve arquivos pessoais de menu para apontar para outra instalação. Atualizar um AppImage não garante substituir um ícone que uma ferramenta externa tenha copiado anteriormente para a pasta pessoal.

## Um painel por sessão gráfica

Uma segunda abertura normal, inclusive de outra cópia portátil/instalada da mesma versão corrigida, solicita que a instância existente apareça e encerra o novo processo. Funciona quando o painel está oculto na bandeja; preserva a maximização da janela. O canal é local, restrito ao mesmo usuário e à sessão gráfica, com trava de processo e confirmação da solicitação. Um processo que falha não deixa uma trava permanente.

Somente o pedido de mostrar a janela é aceito. Argumentos de execução de testes, comandos de sistema, drivers e clocks não são encaminhados. Se um teste real por linha de comando for solicitado com o painel já aberto, retorna erro em vez de anunciar um teste inexistente. A solicitação de foco respeita as regras do compositor. Prévias offline e verificações automatizadas continuam isoladas e não ativam o painel em uso. Sessões gráficas distintas não disputam a mesma instância.

Ao atualizar a partir de uma versão anterior à 0.6.1, encerre uma vez as cópias antigas pela bandeja antes de abrir a nova versão: executáveis antigos ainda não participam do protocolo de instância única. Nenhum processo já aberto é encerrado à força pelo atualizador.
