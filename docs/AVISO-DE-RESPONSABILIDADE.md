# Termo de responsabilidade e assunção de riscos — Overclock

Versão do termo: 2 · GPU-PRO 0.8.0

Antes de usar a aba **Overclock**, leia este termo com atenção. Ao clicar em **Aceitar**, você
declara que o leu, que o compreendeu e que concorda com ele.

## 1. O que esta função faz

A aba Overclock altera clocks de núcleo e de memória, o limite de potência, o deslocamento de
tensão e a velocidade das ventoinhas, somente nas faixas que o driver expõe para a GPU
selecionada. Nada é aplicado ao abrir a aba. Cada ajuste pede confirmação e autorização do
sistema (pkexec no Linux, administrador no Windows). O app confere o valor lendo de volta do driver
e oferece a restauração dos valores padrão.

## 2. Riscos que você assume

- **Danos ao equipamento.** Clocks acima do especificado, tensão alterada, limite de potência
  elevado ou ventoinhas baixas podem causar superaquecimento, degradação acelerada e dano
  permanente à GPU, à fonte, à placa-mãe e a outros componentes.
- **Instabilidade e perda de dados.** Ajustes podem causar travamentos, telas pretas,
  reinicializações, falhas de driver e corrupção de arquivos ou de trabalhos não salvos.
- **Garantia.** Alterações fora das especificações podem afetar a cobertura de garantia, conforme
  os termos do fabricante e a legislação aplicável.
- **Leituras aproximadas.** Sensores, gráficos e testes de estabilidade dependem do driver. Eles
  ajudam a acompanhar o comportamento, mas não garantem estabilidade em jogos ou em uso
  prolongado. Os ajustes manuais não têm o watchdog do diagnóstico curto.
- **Uso indevido.** Aplicar valores sem acompanhar temperatura e estabilidade, ignorar os avisos
  do app ou usar o GPU-PRO em equipamentos de terceiros sem autorização é de sua responsabilidade.

## 3. Fundamento legal

- **Constituição Federal, art. 5º, II:** ninguém é obrigado a fazer ou deixar de fazer alguma
  coisa senão em virtude de lei. Alterar a configuração do próprio hardware é escolha livre do
  usuário. Nenhum ajuste é aplicado sem a sua ação e a sua autorização.
- **Código Civil, arts. 186 e 927:** quem, por ação ou omissão voluntária, negligência ou
  imprudência, causar dano a outrem comete ato ilícito e fica obrigado a repará-lo. Danos
  decorrentes de ajustes escolhidos pelo usuário são de responsabilidade de quem os realizou.
- **Código Civil, art. 945:** a participação da própria vítima no evento danoso é considerada na
  fixação da responsabilidade.
- **Código de Defesa do Consumidor, art. 12, § 3º, III, e art. 14, § 3º, II:** o fornecedor não é
  responsabilizado quando provar que o dano decorreu de culpa exclusiva do consumidor ou de
  terceiro.
- **Lei nº 9.609/1998 (Lei do Software) e licença do GPU-PRO:** o programa é fornecido no estado
  em que se encontra, sem garantia de adequação a um fim específico. A licença acompanha o
  aplicativo.

## 4. Declaração do usuário

Ao aceitar, você declara que:

1. realiza os ajustes por vontade própria, por sua conta e risco, no seu próprio equipamento ou
   com autorização do proprietário;
2. fez ou fará backup dos seus dados e acompanhará temperatura, consumo e estabilidade, usando a
   restauração quando necessário;
3. reconhece que o desenvolvedor e os colaboradores do GPU-PRO não respondem por danos ao
   equipamento, perda de dados, instabilidade ou perda de garantia decorrentes dos ajustes que
   você escolher realizar, por se tratar de culpa exclusiva do usuário (CDC, art. 12, § 3º, III,
   e art. 14, § 3º, II; Código Civil, arts. 186, 927 e 945);
4. respeitará as especificações e os limites indicados pelo fabricante do equipamento.

## 5. Limites deste termo

Este termo não exclui nem reduz direitos que a lei garante e que não podem ser afastados por
contrato (CDC, arts. 25 e 51, I). O desenvolvedor continua respondendo por aquilo que a lei não
permite excluir. Este texto é informativo e não substitui orientação jurídica individual.

## 6. Registro do aceite

O aceite fica registrado somente neste computador, com data e versão do termo (`termos.json`
na pasta de configuração do GPU-PRO), e no Log de operações. Nada é transmitido. Se você recusar,
a aba Overclock fica indisponível e as demais funções continuam disponíveis. O termo volta a
aparecer sempre que você abrir a aba, a menos que marque **Não exibir novamente nesta versão**.
