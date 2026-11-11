#include "single_instance.h"
#include <unistd.h>
SingleInstance::SingleInstance(QString folder, QString session, QObject *parent)
    : QObject(parent), directory(std::move(folder)) {
  const auto hash =
      QCryptographicHash::hash(session.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
  endpoint = directory + "/panel-" + hash;
  lock = std::make_unique<QLockFile>(endpoint + ".lock");
  lock->setStaleLockTime(0);
  server.setSocketOptions(QLocalServer::UserAccessOption);
  connect(&server, &QLocalServer::newConnection, this, [this] {
    while (server.hasPendingConnections()) {
      auto *socket = server.nextPendingConnection();
      socket->setParent(this);
      connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
      auto read = [this, socket] {
        if (socket->bytesAvailable() > 32) {
          socket->abort();
          return;
        }
        if (!socket->canReadLine())
          return;
        if (socket->readLine(33) != "ACTIVATE\n") {
          socket->abort();
          return;
        }
        socket->write("OK\n");
        socket->flush();
        socket->disconnectFromServer();
        pending = true;
        emit activationRequested();
      };
      connect(socket, &QLocalSocket::readyRead, this, read);
      QTimer::singleShot(1500, socket, [socket] {
        socket->abort();
        socket->deleteLater();
      });
      read();
    }
  });
}
QString SingleInstance::runtimeDirectory() {
  return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/gpu-pro";
}
QString SingleInstance::graphicalSession() {
  QByteArray display = qgetenv("WAYLAND_DISPLAY");
  if (display.isEmpty())
    display = qgetenv("DISPLAY");
  return QString::number(geteuid()) + ":" + QString::fromLocal8Bit(display);
}
SingleInstance::Result SingleInstance::acquire() {
  const bool existed = QFileInfo::exists(directory);
  if (!QDir().mkpath(directory)) {
    errorText = "Não foi possível criar o diretório da sessão";
    return Failed;
  }
  if (!existed)
    QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
  QFileInfo folder(directory);
  if (folder.isSymLink() || folder.ownerId() != geteuid() ||
      (folder.permissions() & (QFile::WriteGroup | QFile::WriteOther))) {
    errorText = "O diretório de comunicação da sessão não é privado";
    return Failed;
  }
  for (int attempt = 0; attempt < 12; ++attempt) {
    if (lock->tryLock(0)) {
      // Only the lock owner may remove an abandoned socket after a crash.
      QLocalServer::removeServer(endpoint);
      if (server.listen(endpoint))
        return Primary;
      errorText = server.errorString();
      lock->unlock();
      return Failed;
    }
    QLocalSocket client;
    client.connectToServer(endpoint);
    if (client.waitForConnected(150)) {
      client.write("ACTIVATE\n");
      client.flush();
      if ((client.canReadLine() || client.waitForReadyRead(1000)) && client.readLine() == "OK\n")
        return ActivatedExisting;
    }
    if (lock->error() == QLockFile::PermissionError)
      break;
    QThread::msleep(80);
  }
  errorText =
      "Outra instância está iniciando ou não respondeu. Nenhuma janela duplicada foi aberta.";
  return Failed;
}
