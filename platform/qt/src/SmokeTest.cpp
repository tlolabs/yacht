#include "SmokeTest.h"
#include "AppIdentity.h"
#include "Preferences.h"
#include "YachtCore.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QThread>
#include <QUuid>

namespace Yacht {

namespace {

void waitWithEvents(int ms) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }
}

} // namespace

int SmokeTest::run(MainWindow &window, const QString &directory) {
    if (qEnvironmentVariableIsEmpty("YACHT_TEST_DATA")) {
        qCritical("UI tests require isolated YACHT_TEST_DATA.");
        return 1;
    }

    QDir dir(directory);
    dir.mkpath(QStringLiteral("."));

    try {
        QString input = dir.filePath(QStringLiteral("Café.csv"));
        QFile inputFile(input);
        if (!inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            throw std::runtime_error("Cannot create input test CSV file.");
        }
        inputFile.write("A,B\n<script>,🛥\n1,2,3\n");
        inputFile.close();

        window.loadFile(input);
        window.setStyle(YachtCore::style(QStringLiteral("unstyled")));
        window.onPerformRender();
        waitWithEvents(250);

        QString source = window.sourceEdit()->toPlainText();
        if (!source.contains(QStringLiteral("&lt;script&gt;")) || source.contains(QStringLiteral("<style>"))) {
            throw std::runtime_error("Preview/source escaping and style");
        }

        if (window.previewWidget()->inspectSmokeDocument() != QStringLiteral("1")) {
            throw std::runtime_error("Native preview DOM does not contain the current escaped, unstyled table");
        }

        window.switchTab(1);
        waitWithEvents(50);
        window.switchTab(0);
        window.onPerformRender();
        waitWithEvents(250);

        if (window.previewWidget()->inspectSmokeDocument() != QStringLiteral("1")) {
            throw std::runtime_error("Native preview did not recover after tab reattachment");
        }

        QString presetName = QStringLiteral("CI-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        window.setPresetNameInput(presetName);
        window.saveCurrentPreset();
        window.selectPreset(presetName);
        window.loadSelectedPreset();

        window.copyHtmlToClipboard();
        QString clip = QApplication::clipboard()->text();
        if (!clip.contains(QStringLiteral("&lt;script&gt;"))) {
            throw std::runtime_error("Native clipboard");
        }

        QString output = dir.filePath(QStringLiteral("out.html"));
        window.exportTo(output, false);
        QFile outFile(output);
        if (!outFile.open(QIODevice::ReadOnly)) {
            throw std::runtime_error("Cannot read exported HTML file");
        }
        QString outText = QString::fromUtf8(outFile.readAll());
        outFile.close();
        if (!outText.contains(QStringLiteral("&lt;script&gt;"))) {
            throw std::runtime_error("Native export");
        }

        QJsonObject batchArgs;
        QJsonArray batchInputs;
        batchInputs.append(input);
        batchArgs[QStringLiteral("inputs")] = batchInputs;
        batchArgs[QStringLiteral("style")] = window.currentStyle();
        QJsonArray batchResults = YachtCore::call(QStringLiteral("batch"), batchArgs).toArray();
        if (batchResults.isEmpty() ||
            (batchResults.first().toObject().contains(QStringLiteral("error")) &&
             !batchResults.first().toObject().value(QStringLiteral("error")).isNull())) {
            throw std::runtime_error("Native batch");
        }

        Preferences prefs = Preferences::load();
        prefs.previewRows = 50;
        prefs.save();
        if (Preferences::load().previewRows != 50 || window.styleFields().size() != 16) {
            throw std::runtime_error("Settings/style controls");
        }

        window.switchTab(1);
        waitWithEvents(50);

        QFile passFile(dir.filePath(QStringLiteral("passed.txt")));
        if (passFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            passFile.write("Qt native startup/open/preview/source/style/preset/clipboard/export/settings/batch/shutdown passed\n");
            passFile.close();
        }
        return 0;
    } catch (const std::exception &e) {
        QFile failFile(dir.filePath(QStringLiteral("failed.txt")));
        if (failFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            failFile.write(e.what());
            failFile.write("\n");
            failFile.close();
        }
        return 1;
    }
}

} // namespace Yacht
