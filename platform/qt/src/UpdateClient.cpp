#include "UpdateClient.h"
#include "AppIdentity.h"
#include "YachtCore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace Yacht {

QString UpdateClient::findUpdaterBinary() {
    QString binName = QStringLiteral("yacht-update");
#if defined(Q_OS_WIN)
    binName += QStringLiteral(".exe");
#endif

    // Explicit developer override via environment variable only
    QString devOverride = qEnvironmentVariable("YACHT_DEV_UPDATER_PATH");
    if (!devOverride.isEmpty()) {
        if (QFile::exists(devOverride)) {
            return devOverride;
        }
        return QString();
    }

    if (qEnvironmentVariable("YACHT_ALLOW_DEV_UPDATER") == QStringLiteral("1")) {
        QString candidate = QDir::currentPath() + QStringLiteral("/target/release/") + binName;
        if (QFile::exists(candidate)) {
            return candidate;
        }
        candidate = QDir::currentPath() + QStringLiteral("/target/debug/") + binName;
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    // In production, resolve ONLY the absolute bundled updater in applicationDirPath()
    QString bundled = QCoreApplication::applicationDirPath() + QLatin1Char('/') + binName;
    if (QFile::exists(bundled)) {
        return bundled;
    }

    // Fail closed: do NOT search CWD or PATH in production
    return QString();
}

QJsonObject UpdateClient::run(const QString &command, const QStringList &arguments,
                              std::function<bool()> cancel) {
    if (AppIdentity::isInternal()) {
        throw std::runtime_error("Production updater unavailable in the internal reference host.");
    }

    QString program = findUpdaterBinary();
    if (program.isEmpty()) {
        throw std::runtime_error("The bundled update helper is missing. Updates cannot be verified or installed.");
    }

    QStringList args;
    args.append(command);
    args.append(arguments);

    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(5000)) {
        throw std::runtime_error("Cannot start update checker: " + process.errorString().toStdString());
    }

    // Coordinate timeouts: 910s for download (matching Rust's 900s timeout), 30s for checks
    int timeoutMs = (command == QStringLiteral("download")) ? 910000 : 30000;
    auto startTime = std::chrono::steady_clock::now();

    while (process.state() == QProcess::Running) {
        if (cancel && cancel()) {
            process.kill();
            process.waitForFinished(3000);
            throw YachtCancelledException();
        }
        process.waitForFinished(100);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > timeoutMs) {
            process.kill();
            process.waitForFinished(5000);
            throw std::runtime_error("The update helper timed out. Retry the update check later.");
        }
    }

    if (process.exitCode() != 0) {
        QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (err.isEmpty()) {
            err = QStringLiteral("The update helper failed with exit code %1").arg(process.exitCode());
        }
        throw std::runtime_error(err.toStdString());
    }

    QByteArray out = process.readAllStandardOutput();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(out, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        throw std::runtime_error("Invalid update helper JSON response: " + err.errorString().toStdString());
    }

    return doc.object();
}

void UpdateClient::verifyInstaller(const QJsonObject &download) {
#if defined(Q_OS_WIN)
    const QString script =
        QStringLiteral("& { $ErrorActionPreference='Stop'; "
                       "$p=$env:TLO_UPDATE_PATH; $s=Get-AuthenticodeSignature -LiteralPath $p; "
                       "if($s.Status -ne 'Valid' -or $s.SignerCertificate.Subject -cne $env:TLO_UPDATE_PUBLISHER){throw 'Invalid Authenticode publisher'}; "
                       "if((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant() -cne $env:TLO_UPDATE_SHA256){throw 'Installer changed after download'}; "
                       "$info=[System.Diagnostics.FileVersionInfo]::GetVersionInfo($p); "
                       "if($info.ProductName -cne 'YACHT'){throw 'Installer application mismatch'}; $v=$info.ProductVersion; "
                       "if($v -ne $env:TLO_UPDATE_VERSION -and $v -ne ($env:TLO_UPDATE_VERSION+'.0')){throw 'Installer version mismatch'} }");

    QString ps = QStringLiteral("powershell.exe");
    QString winDir = qEnvironmentVariable("WINDIR");
    if (!winDir.isEmpty()) {
        QString fullPs = winDir + QStringLiteral("\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
        if (QFile::exists(fullPs)) {
            ps = fullPs;
        }
    }

    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TLO_UPDATE_PATH"), download.value(QStringLiteral("path")).toString());
    env.insert(QStringLiteral("TLO_UPDATE_PUBLISHER"), download.value(QStringLiteral("publisher")).toString());
    env.insert(QStringLiteral("TLO_UPDATE_SHA256"), download.value(QStringLiteral("sha256")).toString());
    env.insert(QStringLiteral("TLO_UPDATE_VERSION"), download.value(QStringLiteral("version")).toString());
    process.setProcessEnvironment(env);

    QStringList args{QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-Command"), script};
    process.start(ps, args);
    if (!process.waitForStarted(5000)) {
        throw std::runtime_error("Cannot start installer verification: " + process.errorString().toStdString());
    }

    if (!process.waitForFinished(300000)) {
        process.kill();
        process.waitForFinished(5000);
        throw std::runtime_error("The installer verification timed out.");
    }

    if (process.exitCode() != 0) {
        QString err = QString::fromUtf8(process.readAllStandardError()).trimmed();
        throw std::runtime_error(err.toStdString());
    }
#else
    Q_UNUSED(download);
#endif
}

} // namespace Yacht
