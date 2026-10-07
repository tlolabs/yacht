#include "AppIdentity.h"
#include "MainWindow.h"
#include "SingleInstance.h"
#include "SmokeTest.h"
#include "YachtCore.h"

#include <QApplication>
#include <QDir>
#include <QStringList>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("YACHT"));
    app.setOrganizationName(QStringLiteral("TLO Labs"));
    app.setApplicationDisplayName(Yacht::AppIdentity::title());

    QString version = QStringLiteral("2.1.2");
    try {
        version = Yacht::YachtCore::call(QStringLiteral("info")).toObject().value(QStringLiteral("version")).toString(version);
    } catch (...) {}
    app.setApplicationVersion(version);

    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QStringLiteral("--ui-smoke-test") && i + 1 < args.size()) {
            Yacht::MainWindow window;
            window.initialize();
            return Yacht::SmokeTest::run(window, args[i + 1]);
        }
    }

    Yacht::SingleInstance singleInstance;
    if (!singleInstance.init(args)) {
        return 0; // Forwarded to running primary instance
    }

    Yacht::MainWindow window;
    window.initialize();

    QObject::connect(&singleInstance, &Yacht::SingleInstance::filesReceived, &window,
                     [&window](const QStringList &files) {
                         window.receive(files);
                         window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
                         window.raise();
                         window.activateWindow();
                     });

    window.show();

    QStringList initialFiles;
    for (int i = 1; i < args.size(); ++i) {
        if (!args[i].startsWith(QLatin1Char('-'))) {
            initialFiles.append(args[i]);
        }
    }
    if (!initialFiles.isEmpty()) {
        window.receive(initialFiles);
    }

    return app.exec();
}
