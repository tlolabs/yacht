#include "Presets.h"
#include "AppIdentity.h"
#include "YachtCore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Yacht {

QJsonObject Presets::load() {
    QString presetFile = AppIdentity::presetPath();

    if (!AppIdentity::isInternal() && !QFile::exists(presetFile)) {
        QString legacyPath = QDir::homePath() + QStringLiteral("/.yacht_presets.json");
        if (QFile::exists(legacyPath)) {
            try {
                QJsonObject args;
                args[QStringLiteral("path")] = legacyPath;
                QJsonObject legacy = YachtCore::call(QStringLiteral("load_presets"), args).toObject();
                if (!legacy.isEmpty()) {
                    QDir().mkpath(QFileInfo(presetFile).dir().path());
                    QJsonObject saveArgs;
                    saveArgs[QStringLiteral("path")] = presetFile;
                    saveArgs[QStringLiteral("presets")] = legacy;
                    YachtCore::call(QStringLiteral("save_presets"), saveArgs);
                }
            } catch (...) {
                // Ignore legacy migration errors
            }
        }
    }

    QJsonObject args;
    args[QStringLiteral("path")] = presetFile;
    return YachtCore::call(QStringLiteral("load_presets"), args).toObject();
}

void Presets::save(const QJsonObject &presets) {
    QString presetFile = AppIdentity::presetPath();
    QDir().mkpath(QFileInfo(presetFile).dir().path());

    QJsonObject args;
    args[QStringLiteral("path")] = presetFile;
    args[QStringLiteral("presets")] = presets;
    YachtCore::call(QStringLiteral("save_presets"), args);
}

} // namespace Yacht
