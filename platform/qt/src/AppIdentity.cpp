#include "AppIdentity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace Yacht {

bool AppIdentity::isInternal() {
#if defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

QString AppIdentity::title() {
    return isInternal() ? QStringLiteral("YACHT — Qt Internal Reference")
                        : QStringLiteral("YACHT");
}

QString AppIdentity::dataDirectory() {
    QString testData = qEnvironmentVariable("YACHT_TEST_DATA");
    if (!testData.isEmpty()) {
        return testData;
    }

    if (isInternal()) {
        return QDir::homePath() + QStringLiteral("/Library/Application Support/YACHT-Qt-Internal");
    }

#if defined(Q_OS_WIN)
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
        return localAppData + QStringLiteral("/YACHT");
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#else
    QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (xdg.isEmpty()) {
        xdg = QDir::homePath() + QStringLiteral("/.config");
    }
    return xdg + QStringLiteral("/yacht");
#endif
}

QString AppIdentity::presetPath() {
#if defined(Q_OS_LINUX)
    if (qEnvironmentVariableIsEmpty("YACHT_TEST_DATA")) {
        QString xdgData = qEnvironmentVariable("XDG_DATA_HOME");
        if (xdgData.isEmpty()) {
            xdgData = QDir::homePath() + QStringLiteral("/.local/share");
        }
        return xdgData + QStringLiteral("/yacht/presets.json");
    }
#endif
    return dataDirectory() + QStringLiteral("/presets.json");
}

QString AppIdentity::channelName() {
    QString userName = qEnvironmentVariable("USER");
    if (userName.isEmpty()) {
        userName = qEnvironmentVariable("USERNAME");
    }
    if (userName.isEmpty()) {
        userName = QDir::home().dirName();
    }
    QByteArray input = (userName + dataDirectory()).toUtf8();
    QString hash = QString::fromLatin1(QCryptographicHash::hash(input, QCryptographicHash::Sha256).toHex()).left(24);
    return QStringLiteral("YACHT-Qt-") + hash;
}

} // namespace Yacht
