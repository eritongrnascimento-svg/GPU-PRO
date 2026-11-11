# Licenças e componentes de terceiros

Revisão técnica em 10/11/2026. Escopo: dependências declaradas no CMake, cabeçalhos usados, bibliotecas compartilhadas dos executáveis Linux e conteúdo do pacote. Não é parecer jurídico nem auditoria de procedência de cada linha/recurso gráfico. A licença própria não substitui análise jurídica dos termos, marcas e direitos de cada recurso original.

## Conclusão

Não foi identificado um conflito obrigatório entre a licença source available do código original e a arquitetura atual. Isso depende de cumprir as licenças de terceiros. O fato de uma biblioteca ser apenas uma dependência não elimina suas condições.

A restrição de redistribuição do GPU-PRO não se aplica às bibliotecas de terceiros. Cada uma preserva os direitos concedidos pela sua licença, inclusive sobre modificações dessas bibliotecas. LICENSE explicita essa precedência.

## Componentes e forma de uso

| Componente | Forma usada | Condições relevantes |
| --- | --- | --- |
| Qt Core, Gui, Widgets, Network, OpenGL e DBus | Bibliotecas compartilhadas do sistema | Módulos utilizados disponíveis sob LGPLv3. Avisos, cópias das licenças GNU aplicáveis e mecanismo de substituição por biblioteca compatível. Não há módulos Qt exclusivamente GPL declarados no CMake atual. |
| GTK3/GDK, GLib, Pango, GdkPixbuf e ATK | Bibliotecas compartilhadas do sistema | Família LGPL; conferir versão/opção por componente. O pacote local declara GTK LGPL-2.1-or-later e Pango LGPL-2.0-or-later. Não transformar essas bibliotecas em código proprietário. |
| Cairo | Biblioteca compartilhada do sistema | Alternativa LGPL-2.1 ou MPL-1.1 conforme avisos do projeto. |
| SQLite | Biblioteca compartilhada | Código disponibilizado em domínio público; não exige tornar o GPU-PRO aberto. |
| libarchive | Biblioteca compartilhada | Licenças permissivas por arquivo, predominantemente BSD; preservar avisos quando distribuir material correspondente. |
| Xlib/libglvnd e loader Vulkan | Bibliotecas compartilhadas do sistema | Licenças permissivas dos projetos; não são drivers fornecidos pelo GPU-PRO. |
| GCC/libstdc++ e libc | Runtime do sistema | Condições do runtime e exceção GCC aplicável; não confundir com exigência de licenciar todo programa C++ sob GPL. |
| FFmpeg | Executável externo por QProcess, arquivos de vídeo e argumentos usuais | A distribuição do FFmpeg mantém LGPL/GPL conforme build. O pacote não inclui nem vincula libavcodec/libavformat/libavutil. Essa separação sustenta tratá-lo como programa independente; não se pode generalizar isso para copiar/vincular seu código. |
| MangoHud, vkBasalt e Gamescope | Programas/camadas externos opcionais nos perfis | Não são copiados para o pacote ou incorporados ao código. Mantêm suas próprias licenças. |
| NVML e drivers dos fabricantes | Biblioteca/API do driver instalado pelo usuário; sysfs/DRM nos demais backends | O pacote não redistribui drivers, NVML ou bindings Python. Uso da API não transfere titularidade do driver. |
| glslang/CMake/moc e ferramentas de build | Ferramentas de desenvolvimento | O shader original é compilado em SPIR-V; o compilador não se torna parte do código original do shader. Respeitar os termos das ferramentas e das exceções aplicáveis a código gerado/cabeçalhos. |

### Evidências de empacotamento

`readelf -d` e `ldd` confirmaram ligação dinâmica para Qt, GTK, Cairo, SQLite, libarchive e demais bibliotecas do sistema. A biblioteca estática `gpu-core` contém apenas código do próprio projeto. O tar.gz/.run/.deb não contêm `.so`, FFmpeg, drivers ou bindings Python. O AppImage contém bibliotecas identificadas em seu manifesto de terceiros e na fonte correspondente fornecida na mesma release.

Esta revisão não substitui uma análise nova se forem adicionados componentes, copiado código de terceiros ou alterada a maneira de empacotar.

## Cumprimento das condições LGPL

Os textos completos GPLv3, LGPLv3 e LGPLv2.1 ficam em `docs/licenses/`. As cópias GNU e os avisos também estão incorporados ao aplicativo, em **Atualizações → Licenças e componentes de terceiros**. O botão Sobre Qt apresenta os avisos do framework.

Os executáveis usam o carregador dinâmico normal. Não há verificação criptográfica ou DRM impedindo substituir bibliotecas compatíveis. A verificação SHA-256 do atualizador confere somente o pacote recebido; não bloqueia bibliotecas modificadas após a instalação.

Para testar bibliotecas compatíveis instaladas em um prefixo próprio, o usuário pode usar um ambiente de desenvolvimento como:

```sh
LD_LIBRARY_PATH=/caminho/qt-compativel/lib QT_PLUGIN_PATH=/caminho/qt-compativel/plugins sh run.sh
```

Também pode recompilar o projeto com `CMAKE_PREFIX_PATH` apontando para o Qt escolhido, respeitando API/ABI e dependências. A interface e os workers não usam setuid. O helper usa Qt Core e exige instalação administrativa; o proprietário do sistema pode recompilar e reinstalar o helper conforme o tutorial. Não há chave de fabricante necessária para essa substituição.

As fontes das bibliotecas instaladas, incluindo patches da distribuição, devem ser obtidas da versão correspondente no fornecedor do pacote. Qt upstream: [qtbase](https://code.qt.io/cgit/qt/qtbase.git/); GTK3: [código e licença](https://gitlab.gnome.org/GNOME/gtk/-/tree/gtk-3-24). Receitas/fontes das distribuições: [Arch Qt6 Base](https://gitlab.archlinux.org/archlinux/packaging/packages/qt6-base), [Debian Qt6 Base](https://sources.debian.org/src/qt6-base/). Para pacotes CachyOS, conferir a receita/patches da versão efetivamente instalada; não presumir identidade binária com o upstream ou Arch.

TAR, RUN e DEB usam as bibliotecas compartilhadas do sistema, mecanismo previsto na seção 4(d)(1) da LGPLv3, sem redistribuí-las nesses formatos. O AppImage inclui bibliotecas: seus avisos e fontes correspondentes acompanham a release, conforme a seção abaixo. Mudanças no empacotamento, bibliotecas modificadas ou ligação estática exigem nova revisão de fontes, avisos, relink e informações de instalação. Um link genérico para fontes pode não satisfazer as obrigações de um componente redistribuído.

## Situações que criariam conflito

- Copiar código GPL para o GPU-PRO ou vincular biblioteca exclusivamente GPL sem licença alternativa adequada, mantendo uma proibição de redistribuição incompatível.
- Aplicar a proibição do código original do GPU-PRO de redistribuir às bibliotecas LGPL/BSD/MIT, ou impedir direitos de modificação/substituição assegurados por essas licenças.
- Distribuir bibliotecas sem os textos/avisos/fontes e demais materiais exigidos para a forma de distribuição escolhida.
- Aceitar contribuição com código de terceiros que o contribuinte não pode licenciar nos termos do projeto.

## Referências primárias

- [LGPLv3, especialmente seções 3 e 4](https://www.gnu.org/licenses/lgpl-3.0.html).
- [Orientação de obrigações da Qt Company](https://www.qt.io/development/open-source-lgpl-obligations).
- [Licença de Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html#licenses) e [Qt OpenGL](https://doc.qt.io/qt-6/qtopengl-index.html#licenses-and-trademarks).
- [GTK3 COPYING](https://github.com/GNOME/gtk/blob/gtk-3-24/COPYING); a versão concreta e os avisos por arquivo prevalecem.
- [SQLite copyright](https://www.sqlite.org/copyright.html), [libarchive COPYING](https://github.com/libarchive/libarchive/blob/master/COPYING).
- [FFmpeg: condições de licença](https://ffmpeg.org/legal.html), [GNU: programas separados/agregação](https://www.gnu.org/licenses/gpl-faq.html#MereAggregation).
- [GCC Runtime Library Exception](https://www.gnu.org/licenses/gcc-exception-3.1.html).


## Distribuição AppImage — desde a versão 0.4

O AppImage muda o modo de distribuição: inclui bibliotecas e mantém seus avisos em `usr/share/doc/gpu-pro/thirdparty`. A release fornece `GPU-PRO-<versão>-dependency-sources.tar.gz` com fontes Ubuntu nas versões exatas mapeadas dos arquivos embarcados, mais os arquivos de copyright e licenças comuns. O build falha se não conseguir mapear uma biblioteca ou obter sua fonte correspondente. Esses componentes continuam redistribuíveis nos termos de suas próprias licenças; a restrição do código original do GPU-PRO não se aplica a eles.

É possível extrair o AppImage com `--appimage-extract` e substituir bibliotecas compatíveis no AppDir, sem assinatura/DRM que impeça essa operação. A edição .deb não agrupa bibliotecas e usa dependências calculadas por `dpkg-shlibdeps`.


## Integrações da versão 0.6

A manutenção de drivers usa o `pacman` externo e não vincula a biblioteca GPL libalpm ao painel nem ao worker. A simulação Arch é uma execução isolada do programa instalado pela distro, com confirmação de instalação recusada; APT/DNF usam comandos nativos de prévia. `pactl` e `bubblewrap` também são executáveis externos não redistribuídos. Não foi incorporada biblioteca exclusivamente GPL ao código original. Qt/LGPL, runtimes e demais bibliotecas mantêm os termos descritos acima; não se afirma ausência de qualquer componente com alternativa GPL ou exceção de runtime.
