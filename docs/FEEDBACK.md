# Feedback e banco de resultados

Schema: `linux-gpu-feedback/1`. Não há comunicação de rede. O aplicativo exporta somente registros locais e importa dados recebidos como referências não verificadas, sem aplicar configurações.

Um registro contém UUID aleatório do teste (para deduplicação), categoria (`graphics`, `video`, `compute`, `bottleneck`, `clock-trial`), validade de execução, versão do método, data UTC, metadados técnicos da GPU, parâmetros e resultados. O UUID não é o identificador da placa.

`valid: true` significa que o protocolo terminou atendendo seus critérios, não que um overclock seja estável indefinidamente. No teste de clocks, exige resultados gráficos, confirmação do helper e restauração; mesmo assim a classificação é de teste curto.

A exportação passa por uma lista explícita de campos permitidos, incluindo dentro de telemetria e resultados aninhados. Dados de GPU usam outra lista explícita: nome, fabricante, device ID, arquitetura, memória/barramento, unidades reportadas, driver, VBIOS, subsystem IDs e clocks máximos reportados. Não há UUID/serial/hostname/usuário, processo/PID, diretórios, argumentos de execução ou logs brutos. Erros locais detalhados não são publicados; códigos e estados estruturados ajudam a identificar falhas.

Antes de salvar, a interface mostra o JSON integral. Arquivos com mais de 16 MiB não são importados. São aceitos no máximo 2000 registros por arquivo, com schema conhecido, UUID de teste válido, categoria reconhecida e validade booleana. Pontuações devem ser números não negativos dentro da faixa prevista. Campos extras são descartados pelo sanitizador. Transações SQLite tornam a importação atômica; IDs existentes não são sobrescritos.

Não há assinatura digital/atestado remoto: alguém pode editar um JSON. Por isso a origem importada nunca recebe o mesmo significado de teste local acompanhado. Não use esses arquivos diretamente para escolher clocks sem validar na placa alvo.

Banco SQLite: tabela `records`, colunas `id`, `kind`, `device`, `created`, `origin`, `data`. `origin` é controlado pelo aplicativo, não pelo arquivo recebido. A chave local do dispositivo é PCI/fabricante/device ID e fica no banco local; não é necessária no feedback. Não depende do plugin Qt SQL.

Para análise futura, agrupe somente métodos e parâmetros equivalentes: API, resolução, duração, shader, driver, variante da placa e, para vídeo, codec/bit depth/preset/cobertura e versão do FFmpeg. Registre falhas e incompatibilidades. Scores de cobertura diferente não são diretamente comparáveis.

Um futuro servidor deve manter essas validações e acrescentar controle de tamanho, autenticação, proveniência, consentimento, retenção e separação entre dados observados e perfis autorizados. Esta entrega não publica serviço nem envia dados a terceiros.

Unidades do protocolo: temperaturas em °C; consumo `power` em W; limites `powerBefore`/`powerAfter` e `powerLimit` em mW; clocks/offsets em MHz; memória em bytes; tempos GPU/CPU em ms; `seconds` em s; `fps` em quadros/s. Campos ausentes/null não são zero: podem representar sensor, feature ou etapa indisponível. As medições computacionais levam sua própria unidade explícita por linha.

Resultados 3D da versão 0.3 usam método e shader novos. `livePreview` informa o modo de visualização; compare a mesma duração, resolução, API, método e modo. Ensaios curtos de QA são identificados por sua duração real.
