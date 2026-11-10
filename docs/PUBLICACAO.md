# Publicação oficial — GPU-PRO

Conta/repositório: **eritongrnascimento-svg/GPU-PRO**. Titular: **Ériton do Nascimento**. Visibilidade pretendida: pública.

## Antes da primeira publicação

- Revisar LICENSE e CONTRIBUTING.md: código disponível, forks para contribuições e auditoria permitidos, redistribuição fora das permissões exige autorização. Esta licença não é MIT/GPL/Apache nem aprovada pela OSI. Termos próprios merecem revisão jurídica antes de serem usados como instrumento de proteção.
- Conferir direitos sobre recursos originais, avisos de terceiros e marca. Não anunciar homologação NVIDIA/AMD/Intel.
- Não enviar diretórios locais `build/`, bancos, logs, credenciais ou resultados brutos. `bin/` não pertence ao histórico Git; binários pertencem às releases.
- Ativar revisão de PRs/proteção da branch principal e relato privado de vulnerabilidades, quando essas opções estiverem disponíveis no plano da conta. Somente o mantenedor deve aprovar e publicar releases.

## Código e CI

Enviar fontes, scripts, arquivos de configuração e documentação para a branch `main`. O workflow `.github/workflows/build.yml` compila e roda verificações offline. Ele tem apenas leitura de conteúdo; PRs não publicam releases e não executam testes de overclock.

O primeiro resultado no GitHub Actions ainda precisa ser observado após o envio. Ele não substitui validação em GPUs reais.

## Criar os pacotes

No checkout compilado:

```sh
sh build.sh
ctest --test-dir build --output-on-failure
sh tools/package.sh build dist
```

O script gera:

- `GPU-PRO-0.3.0-linux-x86_64.tar.gz`
- `GPU-PRO-0.3.0-source.tar.gz`
- `SHA256SUMS.txt`

O tar contém uma pasta `GPU-PRO/`. `release.json` deve coincidir com a versão/tag e a arquitetura. Ajuste a versão no CMake, main.cpp, diagnósticos, manifesto e changelog antes de uma nova entrega. Preserve os métodos de benchmark ou altere seu identificador quando mudar a carga.

## Release

Criar a tag `v0.3.0` sobre o commit revisado, publicar notas com mudanças e limites e anexar os três arquivos. O canal do atualizador considera releases estáveis, não draft/prerelease, e espera exatamente os nomes acima. Uma release inicial não implica validação de todas as GPUs.

O pacote binário precisa indicar a distribuição, Qt/glibc de origem e arquiteturas efetivamente testadas. Não afirmar compatibilidade universal. Para outros ambientes, orientar compilação local.

## Atualizador

Consulta `https://api.github.com/repos/eritongrnascimento-svg/GPU-PRO/releases/latest`, sem token ou envio de telemetria. Busca assets no mesmo repositório/tag, confere SHA-256 e manifesto, rejeita links/caminhos perigosos e impõe limites de tamanho. Salva a instalação antes de substituir arquivos; falhas de cópia acionam tentativa de rollback. A restauração manual permanece disponível nos arquivos de backup.

Não é uma transação atômica do pacote inteiro: desligamento no meio da cópia pode exigir restauração manual. Os testes locais verificam instalação/backup/restauração em diretórios temporários e recusas de pacotes inválidos; entrega por uma release real só pode ser exercitada depois de existir uma versão posterior publicada.

Checksum da mesma release oferece verificação de integridade, sem assinatura independente. A proteção contra comprometimento da conta exige uma evolução futura do esquema de assinatura. O helper root nunca é atualizado silenciosamente.
