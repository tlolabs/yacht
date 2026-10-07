#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace Yacht {

class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;

    bool init(const QStringList &arguments);

signals:
    void filesReceived(const QStringList &paths);

private slots:
    void onNewConnection();
    void onFlushQueue();

private:
    QLocalServer *m_server{nullptr};
    QString m_channel;
    QStringList m_queuedFiles;
    QTimer m_debounceTimer;
};

} // namespace Yacht
