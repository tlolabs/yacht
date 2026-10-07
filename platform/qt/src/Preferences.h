#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace Yacht {

class Preferences {
public:
    bool rememberStyle{true};
    int previewRows{200};
    QString appearance{QStringLiteral("System")};
    QStringList recent;
    QJsonObject lastStyle;
    bool readable{true};

    static Preferences load();
    static Preferences importLinuxPreferences(const QString &path);
    bool save() const;
};

} // namespace Yacht
