#include "window.h"
#include "branding.h"
#include "single_instance.h"
#include <cstring>
#include <cstdio>
int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i)
    if (std::strcmp(argv[i], "--overlay-child") == 0)
      return overlayChild(argc, argv);
  // Keep rendering independent of Qt/Kvantum platform theme plugins.
  const auto originalStyle=qgetenv("QT_STYLE_OVERRIDE"), originalTheme=qgetenv("QT_QPA_PLATFORMTHEME");
  const bool hadStyle=qEnvironmentVariableIsSet("QT_STYLE_OVERRIDE"),hadTheme=qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME");
  qputenv("QT_STYLE_OVERRIDE", "Fusion");
  qunsetenv("QT_QPA_PLATFORMTHEME");
  QApplication::setDesktopSettingsAware(false);
  QApplication app(argc, argv);
  app.setStyle(QStyleFactory::create("Fusion"));
  app.setDesktopFileName("gpu-pro");
  QPalette fixed;
  fixed.setColor(QPalette::Window,QColor("#101014"));
  fixed.setColor(QPalette::WindowText,QColor("#eeeeee"));
  fixed.setColor(QPalette::Base,QColor("#17171d"));
  fixed.setColor(QPalette::AlternateBase,QColor("#202027"));
  fixed.setColor(QPalette::Text,QColor("#eeeeee"));
  fixed.setColor(QPalette::Button,QColor("#24242b"));
  fixed.setColor(QPalette::ButtonText,QColor("#eeeeee"));
  fixed.setColor(QPalette::Highlight,QColor("#76b900"));
  fixed.setColor(QPalette::HighlightedText,Qt::black);
  app.setPalette(fixed);
  // Child games/tools retain the user's theme environment.
  if(hadStyle)qputenv("QT_STYLE_OVERRIDE",originalStyle);else qunsetenv("QT_STYLE_OVERRIDE");
  if(hadTheme)qputenv("QT_QPA_PLATFORMTHEME",originalTheme);else qunsetenv("QT_QPA_PLATFORMTHEME");
  QCoreApplication::setOrganizationName("LinuxGPUControl");
  QCoreApplication::setApplicationName("LinuxGPUControl");
  QCoreApplication::setApplicationVersion("0.6.1");
  QCommandLineParser parser;
  parser.setApplicationDescription(
      "GPU-PRO — painel MultiGPU Linux");
  parser.addOption({"interaction-check", "Verificar HUD ao vivo, estilo fixo e aceite sem acessar GPU"});
  parser.addOption({"ui-check", "Verificar abas e alcance da rolagem sem acessar GPU"});
  parser.addOption({"tray-exit-check", "Verificar saída pela bandeja com janela oculta, sem acessar GPU"});
  parser.addOption({"preview-scroll-bottom", "Rolar a página da prévia até o final"});
  parser.addOption({"render-branding-preview", "Exportar prévia dos ícones sem acessar GPU", "path"});
  parser.addOption({"branding-check", "Verificar transparência e cores dos ícones sem acessar GPU"});
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
      parser.isSet("interaction-check") || parser.isSet("preview-offline") || parser.isSet("ui-check") || parser.isSet("tray-exit-check") || parser.isSet("ui-smoke") || parser.isSet("view-report");
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
  if(parser.isSet("render-branding-preview"))return Branding::writePreview(parser.value("render-branding-preview"))?0:8;
  if(parser.isSet("branding-check")) {
    const auto art=Branding::transparentArtwork();
    if(art.isNull()||qAlpha(art.pixel(0,0))!=0)return 8;
    const auto green=Branding::trayIcon(QColor("#76b900")).pixmap(32).toImage();
    const auto red=Branding::trayIcon(QColor("#ed1c24")).pixmap(32).toImage();
    const auto pulse=Branding::trayIcon(QColor("#76b900"),0).pixmap(32).toImage();
    if(green==red||green==pulse||qAlpha(green.pixel(0,0))!=0)return 8;
    return 0;
  }
  std::unique_ptr<SingleInstance> instance;
  if(!preview) {
    instance=std::make_unique<SingleInstance>(SingleInstance::runtimeDirectory(),SingleInstance::graphicalSession());
    auto result=instance->acquire();
    if(result==SingleInstance::ActivatedExisting) {
      if(parser.isSet("clock-check") || parser.isSet("benchmark-check")) {
        std::fputs("GPU-PRO já está aberto; o teste não foi executado nem encaminhado.\n",stderr);return 10;
      }
      return 0;
    }
    if(result==SingleInstance::Failed){QMessageBox::warning(nullptr,"GPU-PRO",instance->error());return 9;}
  }
  MainWindow window(preview);
  if(instance) {
    QObject::connect(instance.get(),&SingleInstance::activationRequested,&window,[&window] {QMetaObject::invokeMethod(&window,"activateExistingInstance",Qt::QueuedConnection);});
    if(instance->takePendingActivation())QMetaObject::invokeMethod(&window,"activateExistingInstance",Qt::QueuedConnection);
  }
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
  if (parser.isSet("tray-exit-check")) {
    window.hide();
    auto *quit = window.findChild<QAction *>("quitFromTray");
    if (!quit) return 6;
    QTimer::singleShot(0, quit, &QAction::trigger);
    QTimer::singleShot(2000, &app, [&app] { app.exit(6); });
  }
  if (parser.isSet("interaction-check")) return window.checkInteractions() ? 0 : 7;
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
