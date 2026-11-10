# Contribuir com o GPU-PRO

Titular e mantenedor: **Ériton do Nascimento**. O projeto usa uma licença própria de código disponível, não uma licença OSI. Leia [LICENSE](LICENSE) antes de usar ou modificar o código.

Forks no GitHub para auditoria, experimentação e contribuições são permitidos. Distribuições independentes e redistribuição de binários exigem autorização escrita. Compartilhe o link das releases oficiais.

## Fluxo

1. Abra uma issue descrevendo o problema ou a proposta; não publique dados privados ou segredos.
2. Crie um fork e uma branch para sua alteração.
3. Compile seguindo [Instalação](docs/INSTALACAO.md) e rode `ctest --test-dir build --output-on-failure`.
4. Abra um pull request com problema, comportamento resultante, validação e GPUs/drivers realmente testados.
5. Aguarde a revisão do mantenedor. Um PR aberto não é uma alteração aceita nem uma release oficial.

## Direitos sobre a contribuição

Você mantém a autoria da sua contribuição. Ao marcar a declaração no pull request, confirma ter direito de enviá-la e concede a Ériton do Nascimento licença mundial, perpétua, irrevogável, não exclusiva e gratuita para usar, reproduzir, modificar, distribuir e sublicenciar essa contribuição como parte do GPU-PRO, inclusive em futuras versões com outros termos. A concessão não transfere sua titularidade e não implica aceitar o PR. Se não concordar, não envie código; você pode relatar o problema sem conceder direitos sobre código que não enviou.

Não inclua código com licença incompatível, arquivos de drivers, segredos, dumps de hardware com identificadores pessoais ou recursos de terceiros sem licença e atribuição adequadas. Mudanças nas condições de contribuição exigem revisão do mantenedor.

## Limites técnicos

- Não adicione escrita de tensão ou de limite de potência.
- Não acrescente privilégios à interface: apenas o helper de clocks usa `pkexec`.
- Não invente sensores, pontuações ou suporte de codec; ausência de leitura não é zero.
- Não aplique clocks importados de feedback nem converta um teste curto em perfil homologado.
- Indique alterações do shader/método que invalidem comparação de benchmarks antigos.

Atualizações e publicação de releases são responsabilidade do mantenedor. Nenhum PR publica automaticamente uma release.
