#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>

namespace Yacht {

class UpdateClient {
public:
    static QJsonObject run(const QString &command, const QStringList &arguments = QStringList(),
                           std::function<bool()> cancel = nullptr);
    static void verifyInstaller(const QJsonObject &download);
    static QString findUpdaterBinary();
};

} // namespace Yacht
