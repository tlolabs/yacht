#include "Preferences.h"
#include "AppIdentity.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

namespace Yacht {

namespace {

QString unescapeGlibKeyFile(const QString &input) {
    static const QRegularExpression regex(QStringLiteral(R"(\\([snrt\\]))"));
    QString out;
    int lastPos = 0;
    auto it = regex.globalMatch(input);
    while (it.hasNext()) {
        auto match = it.next();
        out.append(input.mid(lastPos, match.capturedStart() - lastPos));
        QString code = match.captured(1);
        if (code == QStringLiteral("s")) out.append(QLatin1Char(' '));
        else if (code == QStringLiteral("n")) out.append(QLatin1Char('\n'));
        else if (code == QStringLiteral("r")) out.append(QLatin1Char('\r'));
        else if (code == QStringLiteral("t")) out.append(QLatin1Char('\t'));
        else out.append(QLatin1Char('\\'));
        lastPos = match.capturedEnd();
    }
    out.append(input.mid(lastPos));
    return out;
}

void sanitize(Preferences &prefs) {
    if (prefs.previewRows != 50 && prefs.previewRows != 200 && prefs.previewRows != 1000) {
        prefs.previewRows = 200;
    }
    if (prefs.appearance != QStringLiteral("System") &&
        prefs.appearance != QStringLiteral("Light") &&
        prefs.appearance != QStringLiteral("Dark")) {
        prefs.appearance = QStringLiteral("System");
    }

    QStringList cleanRecent;
    for (const QString &item : prefs.recent) {
        QString trimmed = item.trimmed();
        if (!trimmed.isEmpty() && !cleanRecent.contains(trimmed)) {
            cleanRecent.append(trimmed);
            if (cleanRecent.size() >= 10) break;
        }
    }
    prefs.recent = cleanRecent;
}

} // namespace

Preferences Preferences::importLinuxPreferences(const QString &path) {
    Preferences prefs;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return prefs;
    }

    bool inUi = false;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('['))) {
            inUi = (line == QStringLiteral("[ui]"));
            continue;
        }
        if (!inUi) continue;

        int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) continue;

        QString key = line.left(eq).trimmed();
        QString val = unescapeGlibKeyFile(line.mid(eq + 1));

        if (key == QStringLiteral("remember_style")) {
            prefs.rememberStyle = (val == QStringLiteral("true"));
        } else if (key == QStringLiteral("preview_rows")) {
            bool ok = false;
            int rows = val.toInt(&ok);
            if (ok) prefs.previewRows = rows;
        } else if (key == QStringLiteral("appearance")) {
            prefs.appearance = val;
        } else if (key == QStringLiteral("recent_files")) {
            QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
            if (doc.isArray()) {
                prefs.recent.clear();
                for (const auto &v : doc.array()) {
                    prefs.recent.append(v.toString());
                }
            }
        } else if (key == QStringLiteral("last_style")) {
            QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
            if (doc.isObject()) {
                prefs.lastStyle = doc.object();
            }
        }
    }

    sanitize(prefs);
    return prefs;
}

Preferences Preferences::load() {
    Preferences prefs;
    QString jsonPath = AppIdentity::dataDirectory() + QStringLiteral("/ui.json");
    QFile file(jsonPath);

    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            prefs.readable = false;
            return prefs;
        }
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            prefs.readable = false;
            return prefs;
        }

        QJsonObject obj = doc.object();

        if (obj.contains(QStringLiteral("RememberStyle"))) {
            prefs.rememberStyle = obj.value(QStringLiteral("RememberStyle")).toBool(true);
        } else if (obj.contains(QStringLiteral("remember_style"))) {
            prefs.rememberStyle = obj.value(QStringLiteral("remember_style")).toBool(true);
        }

        if (obj.contains(QStringLiteral("PreviewRows"))) {
            prefs.previewRows = obj.value(QStringLiteral("PreviewRows")).toInt(200);
        } else if (obj.contains(QStringLiteral("preview_rows"))) {
            prefs.previewRows = obj.value(QStringLiteral("preview_rows")).toInt(200);
        }

        if (obj.contains(QStringLiteral("Appearance"))) {
            prefs.appearance = obj.value(QStringLiteral("Appearance")).toString(QStringLiteral("System"));
        } else if (obj.contains(QStringLiteral("appearance"))) {
            prefs.appearance = obj.value(QStringLiteral("appearance")).toString(QStringLiteral("System"));
        }

        QJsonArray recArr;
        if (obj.contains(QStringLiteral("Recent"))) {
            recArr = obj.value(QStringLiteral("Recent")).toArray();
        } else if (obj.contains(QStringLiteral("recent"))) {
            recArr = obj.value(QStringLiteral("recent")).toArray();
        }
        prefs.recent.clear();
        for (const auto &val : recArr) {
            prefs.recent.append(val.toString());
        }

        if (obj.contains(QStringLiteral("LastStyle"))) {
            prefs.lastStyle = obj.value(QStringLiteral("LastStyle")).toObject();
        } else if (obj.contains(QStringLiteral("last_style"))) {
            prefs.lastStyle = obj.value(QStringLiteral("last_style")).toObject();
        }

        sanitize(prefs);
        return prefs;
    }

#if defined(Q_OS_LINUX)
    QString iniPath = AppIdentity::dataDirectory() + QStringLiteral("/ui.ini");
    if (QFile::exists(iniPath)) {
        return importLinuxPreferences(iniPath);
    }
#endif

    sanitize(prefs);
    return prefs;
}

bool Preferences::save() const {
    if (!readable) {
        return false;
    }

    QDir().mkpath(AppIdentity::dataDirectory());

    QJsonObject obj;
    obj[QStringLiteral("RememberStyle")] = rememberStyle;
    obj[QStringLiteral("PreviewRows")] = previewRows;
    obj[QStringLiteral("Appearance")] = appearance;

    QJsonArray recArr;
    for (const QString &path : recent) {
        recArr.append(path);
    }
    obj[QStringLiteral("Recent")] = recArr;

    if (!lastStyle.isEmpty()) {
        obj[QStringLiteral("LastStyle")] = lastStyle;
    }

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Indented);

    QString tempPath = AppIdentity::dataDirectory() + QStringLiteral("/ui-") +
                       QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".tmp");
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (tempFile.write(data) != data.size()) {
        tempFile.close();
        tempFile.remove();
        return false;
    }
    tempFile.close();

    QString targetPath = AppIdentity::dataDirectory() + QStringLiteral("/ui.json");
    QFile::remove(targetPath);
    if (!QFile::rename(tempPath, targetPath)) {
        tempFile.remove();
        return false;
    }
    return true;
}

} // namespace Yacht
