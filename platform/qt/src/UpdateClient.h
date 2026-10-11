#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Yacht {

class UpdateClient {
public:
    static QJsonObject run(const QString &command, const QStringList &arguments = QStringList());
    static void verifyInstaller(const QJsonObject &download);
};

} // namespace Yacht
