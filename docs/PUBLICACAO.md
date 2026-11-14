# Como são preparadas as releases oficiais

Publico o GPU-PRO no repositório público **eritongrnascimento-svg/GPU-PRO**, sob titularidade de **Ériton do Nascimento**. Esta página descreve o processo de construção e distribuição para quem deseja auditar, contribuir ou reproduzir os pacotes.

## Revisão e segurança

- LICENSE e CONTRIBUTING.md definem código disponível, forks de auditoria/contribuição e restrições de redistribuição. A licença própria não é MIT/GPL/Apache nem aprovada pela OSI; sua adequação jurídica exige análise especializada.
- NOTICE, docs/licenses/ e [LICENCAS.md](LICENCAS.md) acompanham a revisão de dependências e bibliotecas empacotadas. Recursos e marcas exigem direitos de uso próprios; o projeto não anuncia homologação NVIDIA/AMD/Intel.
- Fontes, scripts, configurações e documentação ficam em `main`. Diretórios `build/`, bancos, logs, credenciais e resultados brutos não pertencem ao histórico público; `bin/` também é excluído do Git. Os binários oficiais ficam nas releases.
- Revisão de PRs, proteção de branch e relatos privados de vulnerabilidades são medidas previstas para a manutenção, conforme as opções disponíveis na conta. Esta página não afirma que todas essas configurações estejam ativadas. A aprovação de contribuições e publicação oficial cabem ao mantenedor.

## Integração contínua

O workflow `.github/workflows/build.yml` compila e executa verificações offline. O workflow `.github/workflows/packages.yml`, acionado manualmente, gera os formatos de instalação no Ubuntu 24.04. Ambos têm permissão de leitura do conteúdo; PRs não publicam releases nem executam overclock. Consulte as [execuções públicas](https://github.com/eritongrnascimento-svg/GPU-PRO/actions) para verificar cada commit. Aprovação no CI não substitui testes em GPUs reais.

O workflow **release.yml** recompila e testa em Ubuntu 24.04 quando a versão de release.json muda em main (ou por acionamento manual). Publica a release somente depois de verificar todos os pacotes e os checksums; usa o token temporário do GitHub Actions. Não inclui capturas locais nem modifica a galeria existente.

## Reproduzir o empacotamento

Em um checkout com as dependências de desenvolvimento instaladas:

```sh
sh build.sh
ctest --test-dir build --output-on-failure
sh tools/package.sh build dist
```

`package.sh` gera o pacote portátil TAR, o código-fonte e `SHA256SUMS.txt`. Para reproduzir também DEB, RUN e AppImage, use o ambiente Ubuntu 24.04 descrito no workflow e execute:

```sh
bash tools/build-distribution.sh build dist
```

Para a versão 0.7.0, os arquivos são:

- `GPU-PRO-0.7.0-linux-x86_64.tar.gz`
- `GPU-PRO-0.7.0-source.tar.gz`
- `GPU-PRO-0.7.0-linux-x86_64.run`
- `GPU-PRO-0.7.0-x86_64.AppImage`
- `gpu-pro_0.7.0_ubuntu24.04_amd64.deb`
- `GPU-PRO-0.7.0-dependency-sources.tar.gz` e `GPU-PRO-0.7.0-dependencies.json`
- `SHA256SUMS.txt`

O TAR contém uma pasta `GPU-PRO/`. A versão do CMake, da aplicação, dos diagnósticos, de `release.json`, do changelog e da tag deve coincidir. Mudanças na carga de benchmark exigem atualizar o identificador do método para preservar comparabilidade. O empacotamento falha se não conseguir obter as fontes correspondentes das bibliotecas incluídas no AppImage.

## Publicação e compatibilidade

Cada tag `v<versão>` aponta para o commit revisado. As notas descrevem mudanças, validação e limites, e os arquivos produzidos são anexados à release. O atualizador considera releases estáveis, excluindo drafts e prereleases, e procura nomes exatos para TAR, manifesto e checksums. A presença de uma release não implica validação em todas as GPUs ou distribuições.

A base dos pacotes, Qt/glibc, arquitetura e limites de compatibilidade devem constar na documentação da versão. Para ambientes incompatíveis, a alternativa é compilar localmente. Nenhum pacote é anunciado como universal para todo Linux.

## Atualizador e backup

A consulta usa `https://api.github.com/repos/eritongrnascimento-svg/GPU-PRO/releases/latest`, sem token ou envio de telemetria. TAR/RUN recebem instalação portátil com validação de origem, tamanho, hash, manifesto e caminhos; links e caminhos perigosos são recusados. Um backup precede a substituição, com tentativa de rollback em falhas de cópia e restauração manual disponível nos arquivos salvos.

A cópia não é uma transação atômica do pacote inteiro: desligamento durante a instalação pode exigir restauração manual. Os testes automatizados verificam instalação, backup, restauração e recusa de pacotes inválidos em diretórios temporários. Um teste de atualização por download entre releases reais é uma verificação separada e precisa ser registrado quando executado.

AppImage e DEB abrem o download oficial; a atualização utiliza a substituição do arquivo ou o gerenciador correspondente, após encerrar o aplicativo. O helper de propriedade do root não é atualizado silenciosamente pelo atualizador portátil.

Os checksums da própria release verificam integridade, mas não são assinaturas independentes nem protegem contra comprometimento da conta do mantenedor. Assinaturas independentes permanecem uma melhoria futura.
