#include "window.h"
#include <cstring>
int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--overlay-child") == 0)
      return overlayChild(argc, argv);
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("LinuxGPUControl");
  QCoreApplication::setApplicationName("LinuxGPUControl");
  QCoreApplication::setApplicationVersion("0.4.0");
  QCommandLineParser parser;
  parser.setApplicationDescription(
      "GPU-PRO — painel MultiGPU Linux");
  parser.addOption({"ui-check", "Verificar abas e alcance da rolagem sem acessar GPU"});
  parser.addOption({"preview-scroll-bottom", "Rolar a página da prévia até o final"});
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption({"preview-offline", "Prévia visual com dados fictícios, sem acessar GPU"});
  parser.addOption(
      {"ui-smoke", "Verificação da interface sem acessar GPU; encerra automaticamente"});
  parser.addOption({"screenshot", "Salvar captura da prévia offline", "path"});
  parser.addOption({"render-hud-preview",
                    "Renderizar HUD com dados fictícios, sem abrir janela ou acessar GPU", "path"});
  parser.addOption(
      {"hud-layout", "Formato da prévia HUD: list / compact / columns", "layout", "list"});
  parser.addOption({"preview-page", "Página da prévia offline (0–9)", "index", "0"});
  parser.addOption({"preview-gpu", "Tema da prévia: 0 NVIDIA / 1 AMD / 2 Intel", "index", "0"});
  parser.addOption({"view-report", "Visualizar resultado JSON salvo, sem acessar GPU", "path"});
  parser.addOption({"export-image", "Exportar imagem do --view-report sem diálogo", "path"});
  parser.addOption({"export-feedback", "Exportar feedback do --view-report sem upload", "path"});
  parser.addOption(
      {"preview-subtab", "Subaba: GPU 0 monitor / 1 especificações; Benchmark 0 cenas / 1 computação / 2 vídeo / 3 gargalo", "index", "0"});
  parser.addOption({"benchmark-check", "Verificar fluxo gráfico real por 6 s, com prévia e JSON", "path"});
  parser.addOption({"clock-check",
                    "Teste curto NVIDIA +15 MHz, com autenticação e restauração; salva JSON",
                    "path"});
  parser.process(app);
  bool preview =
      parser.isSet("preview-offline") || parser.isSet("ui-check") || parser.isSet("ui-smoke") || parser.isSet("view-report");
  if (parser.isSet("render-hud-preview"))
    return renderOverlayPreview(parser.value("render-hud-preview"), parser.value("hud-layout")) ? 0
                                                                                                : 1;
  if (parser.isSet("screenshot") && !preview)
    parser.showHelp(2);
  if ((parser.isSet("export-image") || parser.isSet("export-feedback")) &&
      !parser.isSet("view-report"))
    parser.showHelp(2);
  if ((preview && (parser.isSet("clock-check") || parser.isSet("benchmark-check"))) || (parser.isSet("clock-check") && parser.isSet("benchmark-check")))
    return 2;
  MainWindow window(preview);
  if (parser.isSet("view-report")) {
    if (!window.viewReport(parser.value("view-report")))
      return 2;
    if (parser.isSet("export-image") && !window.exportViewedImage(parser.value("export-image")))
      return 3;
    if (parser.isSet("export-feedback") &&
        !window.exportViewedFeedback(parser.value("export-feedback")))
      return 4;
  }
  if (preview) {
    window.previewPage(parser.value("preview-page").toInt());
    window.previewGpu(parser.value("preview-gpu").toInt());
    window.previewSubtab(parser.value("preview-subtab").toInt());
  }
  window.show();
  if (parser.isSet("ui-check")) return window.checkInterface() ? 0 : 5;
  if (parser.isSet("benchmark-check")) window.benchmarkCheck(parser.value("benchmark-check"));
  if (parser.isSet("clock-check"))
    window.clockCheck(parser.value("clock-check"));
  if (parser.isSet("ui-smoke")) {
    QTimer::singleShot(500, &app, [&] {
      if (parser.isSet("preview-scroll-bottom")) window.previewScrollBottom();
      if (parser.isSet("screenshot"))
        window.grab().save(parser.value("screenshot"));
      app.quit();
    });
  }
  return app.exec();
}
