#include "AppIdentity.h"
#include "MainWindow.h"
#include "SingleInstance.h"
#include "SmokeTest.h"
#include "YachtCore.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QStringList>

int main(int argc, char *argv[]) {
    // Opt-in smoke instrumentation starts at main entry, before QApplication.
    QElapsedTimer launchTimer;
    launchTimer.start();
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("YACHT"));
    app.setOrganizationName(QStringLiteral("TLO Labs"));
    app.setApplicationDisplayName(Yacht::AppIdentity::title());

    QString version = QStringLiteral("2.1.3");
    try {
        version = Yacht::YachtCore::call(QStringLiteral("info")).toObject().value(QStringLiteral("version")).toString(version);
    } catch (...) {}
    app.setApplicationVersion(version);

    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QStringLiteral("--ui-smoke-test") && i + 1 < args.size()) {
            Yacht::MainWindow window;
            window.initialize();
            const bool performance = !qEnvironmentVariableIsEmpty("YACHT_PERF_SAMPLES");
            if (performance) window.show();
            return Yacht::SmokeTest::run(window, args[i + 1], performance ? &launchTimer : nullptr);
        }
    }

    QStringList initialFiles = Yacht::SingleInstance::normalizeArguments(args);

    Yacht::SingleInstance singleInstance;
    if (!singleInstance.init(args)) {
        return 0; // Forwarded to running primary instance
    }

    Yacht::MainWindow window;
    window.initialize();

    QObject::connect(&singleInstance, &Yacht::SingleInstance::filesReceived, &window,
                     [&window](const QStringList &files) {
                         if (!files.isEmpty()) {
                             window.receive(files);
                         }
                         window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
                         window.raise();
                         window.activateWindow();
                     });

    window.show();

    if (!initialFiles.isEmpty()) {
        window.receive(initialFiles);
    }

    return app.exec();
}
