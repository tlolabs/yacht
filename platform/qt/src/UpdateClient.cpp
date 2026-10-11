#include "UpdateClient.h"
#include "AppIdentity.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <stdexcept>

namespace Yacht {

namespace {

QString findUpdaterBinary() {
    QString binName = QStringLiteral("yacht-update");
#if defined(Q_OS_WIN)
    binName += QStringLiteral(".exe");
#endif

    QString candidate = QCoreApplication::applicationDirPath() + QLatin1Char('/') + binName;
    if (QFile::exists(candidate)) {
        return candidate;
    }

    // Development locations
    candidate = QDir::currentPath() + QStringLiteral("/target/release/") + binName;
    if (QFile::exists(candidate)) {
        return candidate;
    }
    candidate = QDir::currentPath() + QStringLiteral("/target/debug/") + binName;
    if (QFile::exists(candidate)) {
        return candidate;
    }

    return binName;
}

} // namespace

QJsonObject UpdateClient::run(const QString &command, const QStringList &arguments) {
    if (AppIdentity::isInternal()) {
        throw std::runtime_error("Production updater unavailable in the internal reference host.");
    }

    QString program = findUpdaterBinary();
    QStringList args;
    args.append(command);
    args.append(arguments);

    QProcess process;
    process.start(program, args);
    if (!process.waitForStarted(5000)) {
        throw std::runtime_error("Cannot start update checker: " + process.errorString().toStdString());
    }

    if (!process.waitForFinished(300000)) { // 5 minutes timeout
        process.kill();
        process.waitForFinished(5000);
        throw std::runtime_error("The update helper timed out. Retry the update check later.");
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
