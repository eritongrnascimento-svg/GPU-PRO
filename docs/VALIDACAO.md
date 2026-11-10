# Roteiro para testes de amigos — AMD e Intel

Os testes locais da entrega foram feitos em uma RTX 3060 Ti; isso não homologa o comportamento em outras placas. Benchmarks e feedback não precisam de root. Não instale o helper para apenas coletar resultados em configuração padrão.

1. Se o binário não abrir por ABI/bibliotecas, recompile com `build.sh`. Confira Qt/OpenGL, Vulkan, GTK, SQLite, FFmpeg e o driver do fabricante.
2. Abra Feedback & Diagnóstico, consulte ferramentas, acesso ao render node e leituras disponíveis. Compare sensores com outra ferramenta do mesmo driver. RPM zero é diferente de ausência de leitura.
3. Confira GPU selecionada e identificação técnica. Em máquina híbrida, confirme que o resultado registra o dispositivo esperado. OpenGL pode recusar uma seleção ambígua; tente Vulkan.
4. Comece pelo teste 3D de 30 s em 720p. Só aumente resolução após verificar memória, temperatura e resultado válido. Não execute várias cargas ao mesmo tempo.
5. Faça a medição computacional. Omissão de FP64/int64 por feature não é pontuação zero. Essa carga exige telemetria térmica confiável nesta implementação; falha por sensor deve ser registrada como falha, sem números inventados.
6. Execute a suíte de vídeo. Reserve pelo menos 1 GiB temporário, confira cada codec em encode/decode, cobertura e erros. Preparação na CPU não é pontuada. Resultados de encoders/presets diferentes não garantem qualidade equivalente.
7. Teste gargalo em condição sem outros programas pesados. Interprete apenas como carga sintética de uma thread, não como diagnóstico universal para todos os jogos.
8. Confira os três registros recentes de cada categoria, selecione um resultado antigo e exporte PNG/JPEG/JSON.
9. Gere feedback, revise o conteúdo mostrado e envie o arquivo manualmente. Não envie senha, UUID/serial ou logs com dados pessoais. O botão já remove os campos privados previstos.
10. Ao importar, confira que o registro é não verificado, não altera o histórico local validado e não aplica clocks.
11. No GamerMonitor, confira três formatos, superior centralizado/cantos/centro/coordenadas, monitor secundário, clique passante, minimizar painel e Alt+Q. Teste janela/borderless/fullscreen separadamente; Wayland/XWayland depende do compositor.
12. Clocks: só testar depois de confirmar sensores e entender os controles. O modo automático curto está limitado ao backend NVIDIA; não inventa perfis AMD/Intel com offsets de outra arquitetura. Nenhuma tensão ou power limit é alterado pelo aplicativo.

O teste de +15 MHz realizado nesta entrega não certifica estabilidade em jogos longos. Curvas de tensão/ventoinha e uma busca automática ampla continuam fora desta versão. Há bloqueios de temperatura e tentativa de restauração, mas falha de driver/kernel ou desligamento pode impedir recuperação por software.
