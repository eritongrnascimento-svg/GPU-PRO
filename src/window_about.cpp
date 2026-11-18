// Aba Sobre (igual à versão Windows): projeto, links do repositório, QR do "cafezinho" (PIX
// estático gerado por tools/pix-qr.py) e um versículo sorteado, também mostrado na barra lateral.
#include "window.h"
#include <QRandomGenerator>

namespace {
const QString repoUrl = "https://github.com/eritongrnascimento-svg/GPU-PRO";
QList<QPair<QString, QString>> loadVerses() {
  QList<QPair<QString, QString>> list;
  QFile file(":/assets/versiculos.txt");
  if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    for (const auto &raw : QString::fromUtf8(file.readAll()).split('\n')) {
      const QString line = raw.trimmed();
      if (line.isEmpty() || line.startsWith('#') || !line.contains('|')) continue;
      list << qMakePair(line.section('|', 0, 0).trimmed(), line.section('|', 1).trimmed());
    }
  if (list.isEmpty()) list << qMakePair(QString("Salmos 23:1"), QString("O Senhor é o meu pastor; nada me faltará."));
  return list;
}
QLabel *paragraph(const QString &text, int size = 14) {
  auto *l = new QLabel(text);
  l->setWordWrap(true);
  l->setTextInteractionFlags(Qt::TextSelectableByMouse);
  auto f = l->font();
  f.setPixelSize(size);
  l->setFont(f);
  return l;
}
}

void MainWindow::showVerse(bool another) {
  static const auto verses = loadVerses();
  if (verse.first.isEmpty() || another) {
    auto pick = verse;
    while (verses.size() > 1 && pick.first == verse.first) pick = verses[QRandomGenerator::global()->bounded(verses.size())];
    if (verses.size() == 1) pick = verses.first();
    verse = pick;
  }
  const QString accent = "#76b900";
  if (sidebarVerse)
    sidebarVerse->setText(QString("<i style='font-size:11px'>“%1”</i><br><b style='color:%2;font-size:11px'>%3</b>")
                              .arg(verse.second.toHtmlEscaped(), accent, verse.first.toHtmlEscaped()));
  if (aboutVerse)
    aboutVerse->setText(QString("<i style='font-size:17px'>“%1”</i><br><b style='color:%2'>%3 · Almeida Revista e Corrigida</b>")
                            .arg(verse.second.toHtmlEscaped(), accent, verse.first.toHtmlEscaped()));
}

QWidget *MainWindow::aboutPage() {
  auto *root = page("Sobre");
  auto *l = qobject_cast<QVBoxLayout *>(root->layout());
  auto *project = new QGroupBox("Sobre o projeto");
  auto *pl = new QVBoxLayout(project);
  pl->addWidget(paragraph("GPU-PRO 0.8.1 · Linux", 18));
  pl->addWidget(paragraph("Monitoramento, testes, overlay e ajustes de GPU para Linux e Windows, com núcleo C++ compartilhado. "
                          "Leituras vêm do driver e do sistema: o que não é exposto aparece como N/D, sem estimativas inventadas. "
                          "Projeto independente, sem coleta de dados nem upload automático."));
  pl->addWidget(paragraph("Desenvolvido por Ériton do Nascimento · licença no arquivo LICENSE que acompanha o aplicativo.", 12));
  auto *links = new QHBoxLayout;
  for (const auto &link : QList<QPair<QString, QString>>{{"Repositório no GitHub", repoUrl},
                                                                 {"Versões e downloads", repoUrl + "/releases"},
                                                                 {"Relatar um problema", repoUrl + "/issues"}}) {
    auto *b = new QPushButton(link.first);
    const QString url = link.second;
    connect(b, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(QUrl(url)); });
    links->addWidget(b);
  }
  links->addStretch();
  pl->addLayout(links);
  l->addWidget(project);

  auto *coffee = new QGroupBox("Cafezinho");
  auto *cl = new QHBoxLayout(coffee);
  auto *qr = new QLabel;
  qr->setPixmap(QPixmap(":/assets/pix-cafezinho.png").scaled(220, 220, Qt::KeepAspectRatio, Qt::FastTransformation));
  qr->setStyleSheet("background:#ffffff;border-radius:10px;padding:6px;");
  qr->setFixedSize(232, 232);
  cl->addWidget(qr, 0, Qt::AlignTop);
  auto *text = new QVBoxLayout;
  text->addWidget(paragraph("Aceita um cafezinho? ☕", 20));
  text->addWidget(paragraph("Bora tomar um café com o desenvolvedor do GPU-PRO? Só tem um detalhe: quem paga a conta é você 😄"));
  text->addWidget(paragraph("Aponte a câmera do app do seu banco para o QR (PIX). Qualquer valor ajuda a manter o projeto, "
                            "os testes em hardware real e as novas versões.", 12));
  text->addWidget(paragraph("Chave PIX: eritongrnascimento@gmail.com", 12));
  auto *copy = new QPushButton("Copiar PIX copia e cola");
  connect(copy, &QPushButton::clicked, this, [this] {
    QFile f(":/assets/pix-cafezinho.txt");
    QString code = f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll()).trimmed() : QString();
    QGuiApplication::clipboard()->setText(code.isEmpty() ? "eritongrnascimento@gmail.com" : code);
    log("PIX copia e cola copiado para a área de transferência.");
  });
  text->addWidget(copy, 0, Qt::AlignLeft);
  text->addStretch();
  cl->addLayout(text, 1);
  l->addWidget(coffee);

  auto *verseBox = new QGroupBox("Versículo");
  auto *vl = new QVBoxLayout(verseBox);
  aboutVerse = new QLabel;
  aboutVerse->setWordWrap(true);
  aboutVerse->setTextFormat(Qt::RichText);
  vl->addWidget(aboutVerse);
  auto *another = new QPushButton("Outro versículo");
  connect(another, &QPushButton::clicked, this, [this] { showVerse(true); });
  vl->addWidget(another, 0, Qt::AlignLeft);
  l->addWidget(verseBox);
  l->addStretch();
  showVerse();
  return root;
}
