#pragma once
#include <QtWidgets>
class NeonCard : public QFrame {
  Q_OBJECT
  QVBoxLayout *content;
  QLabel *heading;
  QColor accent{"#76b900"};
  QElapsedTimer animation;
  bool available = false;
public:
  explicit NeonCard(QString title, QWidget *parent = nullptr);
  QVBoxLayout *body() const { return content; }
  void setCompact(bool compact);
  void setReading(const QJsonObject &, QString key, QColor base);
protected:
  void paintEvent(QPaintEvent *) override;
};
