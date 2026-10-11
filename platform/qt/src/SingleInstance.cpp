#include "SingleInstance.h"
#include "AppIdentity.h"

#include <QDataStream>
#include <QJsonArray>
#include <QJsonDocument>

namespace Yacht {

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent), m_channel(AppIdentity::channelName()) {
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);
    connect(&m_debounceTimer, &QTimer::timeout, this, &SingleInstance::onFlushQueue);
}

SingleInstance::~SingleInstance() {
    if (m_server) {
        m_server->close();
    }
}

bool SingleInstance::init(const QStringList &arguments) {
    QStringList files;
    for (const QString &arg : arguments) {
        if (!arg.startsWith(QLatin1Char('-'))) {
            files.append(arg);
        }
    }

    QLocalSocket socket;
    socket.connectToServer(m_channel);
    if (socket.waitForConnected(500)) {
        // Connected to an existing primary instance. Forward files and exit.
        QJsonArray arr;
        for (const QString &f : files) {
            arr.append(f);
        }
        QByteArray payload = QJsonDocument(arr).toJson(QJsonDocument::Compact);
        qint32 size = static_cast<qint32>(payload.size());

        socket.write(reinterpret_cast<const char *>(&size), sizeof(size));
        socket.write(payload);
        socket.flush();
        socket.waitForBytesWritten(3000);
        socket.disconnectFromServer();
        return false;
    }

    // No existing server found. Clean up any stale socket and become primary.
    QLocalServer::removeServer(m_channel);
    m_server = new QLocalServer(this);
    if (!m_server->listen(m_channel)) {
        // If listen fails, proceed anyway so app runs
        return true;
    }

    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection() {
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket *client = m_server->nextPendingConnection();
        connect(client, &QLocalSocket::readyRead, this, [this, client]() {
            if (client->bytesAvailable() < static_cast<qint64>(sizeof(qint32))) {
                return;
            }
            qint32 size = 0;
            if (client->peek(reinterpret_cast<char *>(&size), sizeof(size)) != sizeof(size)) {
                return;
            }
            if (size <= 0 || size > 131072) {
                client->close();
                client->deleteLater();
                return;
            }
            if (client->bytesAvailable() < static_cast<qint64>(sizeof(qint32) + size)) {
                return;
            }
            client->read(reinterpret_cast<char *>(&size), sizeof(size));
            QByteArray payload = client->read(size);

            QJsonDocument doc = QJsonDocument::fromJson(payload);
            if (doc.isArray()) {
                for (const auto &v : doc.array()) {
                    QString path = v.toString();
                    if (!path.isEmpty() && !path.startsWith(QLatin1Char('-'))) {
                        m_queuedFiles.append(path);
                    }
                }
                m_debounceTimer.start();
            }
            client->close();
            client->deleteLater();
        });
        connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
    }
}

void SingleInstance::onFlushQueue() {
    if (!m_queuedFiles.isEmpty()) {
        QStringList batch = m_queuedFiles;
        m_queuedFiles.clear();
        emit filesReceived(batch);
    }
}

} // namespace Yacht
