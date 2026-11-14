# Configurar o overlay

Em **GamerMonitor & Overlay**, ative **Exibir HUD** e escolha os campos, formato, monitor, posição, fonte, largura e fundo. As mudanças são aplicadas automaticamente após uma pequena pausa de aproximadamente 120 ms; não é necessário desativar e reativar o HUD. O botão Aplicar continua disponível. A configuração é salva localmente.

O HUD usa um processo separado em GTK/X11 e recebe snapshots por UDP local. Pode continuar visível quando o painel está na bandeja. Encerrar o aplicativo também encerra seu overlay. O comportamento sobre jogos, fullscreen e XWayland depende do compositor; o HUD não captura cliques. Alt+Q alterna sua visibilidade quando o atalho está disponível.

## Uso e temperatura da CPU

Marque **Uso CPU** e **Temperatura CPU** na lista de métricas. O uso é global, calculado por diferenças dos contadores de `/proc/stat`, e não o uso do processo acompanhado. A primeira leitura precisa de duas amostras; até lá aparece N/D. A coleta ocorre aproximadamente uma vez por segundo.

Temperaturas são lidas de sensores CPU reconhecidos em hwmon, como coretemp, k10temp e zenpower, com fallback para zonas térmicas identificadas como CPU/pacote/SoC. Tdie e pacote têm preferência; quando há canais equivalentes, é exibido o maior valor válido. A origem permanece na telemetria local. Se o kernel não expuser um canal reconhecido, aparece N/D. Não é estimada a temperatura da CPU a partir da GPU.

## FPS atual, 1% low, média e máxima

Selecione **FPS atual (CSV)**, **1% low (amostras CSV)**, **FPS médio (CSV)** e **FPS máximo (CSV)**. A seleção controla as linhas de FPS do GamerMonitor e os campos do HUD. A fonte é o CSV ativo escolhido abaixo, mesmo quando a linha no HUD está desligada. Não é necessário exibir o HUD para acompanhar os valores no painel.

As estatísticas usam as últimas 240 amostras válidas. FPS atual = 1000 / último frametime em ms; média = 1000 × quantidade / soma dos frametimes; máxima = 1000 / menor frametime. O 1% low usa a média dos tempos do 1% mais lento, arredondando a quantidade para cima; só aparece após 100 amostras. Logs por intervalo, como MangoHud log_interval=100, geram **estatísticas das amostras**, não um 1% low por quadro nem resultados de toda a sessão. Para dados por quadro, o produtor precisa gravar cada quadro.

O GamerMonitor também exibe o gráfico em ms. Sem fonte válida ou depois de aproximadamente 3 s sem novidades, FPS fica N/D e o gráfico aguarda dados. Trocar a fonte, truncar o arquivo ou receber outro cabeçalho limpa a série anterior. Nenhum FPS é inferido da utilização da GPU ou do ritmo de atualização da interface.

## Linha de frametime

A linha representa tempos de quadros em milissegundos. Ative **Linha de frametime** e use **Selecionar CSV** para indicar o arquivo que o jogo está gravando. O leitor reconhece uma coluna chamada `frametime`, aceita somente valores finitos e positivos e acompanha novas linhas completas. Mantém até 240 amostras; não substitui frametime por uso de CPU/GPU ou intervalo de atualização do HUD.

Uma fonte opcional é o log do [MangoHud](https://github.com/flightlessmango/MangoHud#fps-logging). Exemplo de opções no arquivo de configuração do MangoHud, usando uma pasta existente e gravável:

```ini
output_folder=/caminho/para/logs
log_interval=100
autostart_log=1
```

Execute o jogo com MangoHud, localize o CSV criado para essa execução e selecione-o no GPU-PRO. O atalho padrão Shift+F2 também pode iniciar/parar o logging, conforme a configuração do MangoHud. Um novo arquivo de outra execução deve ser selecionado novamente. O GPU-PRO não ativa logging ou uploads no MangoHud e não modifica arquivos de configuração de terceiros.

Sem arquivo, cabeçalho compatível ou amostras novas, o gráfico mostra **aguardando CSV ativo**. Após aproximadamente três segundos sem dados novos, a linha é limpa. Logs antigos não são apresentados como dados atuais. A frequência real depende do produtor do CSV e não equivale necessariamente a uma amostra por quadro.

CPU, GPU e configuração ao vivo não exigem MangoHud. Somente essa fonte opcional de frametime usa o log externo. FSR/DLSS/geração de quadros continuam dependendo dos jogos e integrações correspondentes.

## Tema e gravação

O aplicativo fixa o estilo Qt Fusion e sua paleta para impedir que Kvantum ou o tema Qt da sessão alterem os controles. Essa escolha vale para o GPU-PRO, sem modificar o tema global ou o ambiente de jogos iniciados pelo painel.

Os cards animados repintam o fundo integralmente. Em **Atualizações**, a opção **Animar luzes neon dos cards** permite desligar o movimento, mantendo cores e valores atualizados. Use-a caso queira reduzir movimento/custo de repintura durante gravações. Isso não promete corrigir falhas externas do driver, compositor ou captura do OBS.
