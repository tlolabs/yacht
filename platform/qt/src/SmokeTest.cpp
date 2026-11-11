#include "SmokeTest.h"
#include "AppIdentity.h"
#include "Preferences.h"
#include "YachtCore.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextTable>
#include <QUuid>
#include <QWindow>

#include <algorithm>
#include <functional>

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


void waitUntil(const std::function<bool()> &ready) {
    QElapsedTimer timeout;
    timeout.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (ready()) return;
        QThread::msleep(1);
    } while (timeout.elapsed() < 20000);
    throw std::runtime_error("Timed out waiting for an exposed, enabled window with the expected preview.");
}

void finishVisibleUpdate(MainWindow &window, const QString &marker = {}) {
    waitUntil([&] {
        auto *browser = window.previewWidget()->findChild<QTextBrowser *>();
        return window.isVisible() && window.isEnabled() && !window.isBusy() &&
               window.windowHandle() && window.windowHandle()->isExposed() &&
               window.previewWidget()->isNavigationComplete() && browser &&
               !browser->document()->isEmpty() &&
               (marker.isEmpty() || (!browser->document()->find(marker).isNull() &&
                                    !window.sourceEdit()->document()->find(marker).isNull()));
    });
    // Complete widget painting while the real native surface is exposed.
    window.repaint();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void writePerformanceJson(const QString &path, const QJsonObject &data) {
    QFile output(path);
    const QByteArray bytes = QJsonDocument(data).toJson(QJsonDocument::Compact);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.flush()) {
        throw std::runtime_error("Cannot write GUI performance evidence.");
    }
}

QJsonObject measureVisibleOperation(const QString &name, int samples,
                                    const std::function<void()> &operation) {
    constexpr int warmups = 3;
    for (int i = 0; i < warmups; ++i) operation();
    QJsonArray raw;
    QList<qint64> sorted;
    for (int i = 0; i < samples; ++i) {
        QElapsedTimer timer;
        timer.start();
        operation();
        const qint64 ns = timer.nsecsElapsed();
        raw.append(ns);
        sorted.append(ns);
    }
    std::sort(sorted.begin(), sorted.end());
    return {{QStringLiteral("name"), name}, {QStringLiteral("unit"), QStringLiteral("ns")},
            {QStringLiteral("samples"), samples}, {QStringLiteral("warmups"), warmups},
            {QStringLiteral("raw_ns"), raw}, {QStringLiteral("median_ns"), sorted[samples / 2]},
            {QStringLiteral("p95_ns"), sorted[(samples * 95 + 99) / 100 - 1]},
            {QStringLiteral("min_ns"), sorted.first()}, {QStringLiteral("max_ns"), sorted.last()}};
}

void recordVisiblePerformance(MainWindow &window, const QDir &directory,
                              const QElapsedTimer &launchTimer) {
    bool valid = false;
    const int samples = qEnvironmentVariableIntValue("YACHT_PERF_SAMPLES", &valid);
    if (!valid || samples < 1 || samples > 1000) {
        throw std::runtime_error("YACHT_PERF_SAMPLES must be between 1 and 1000.");
    }
    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("offscreen") || platform == QStringLiteral("minimal")) {
        throw std::runtime_error("GUI latency instrumentation requires a real native display surface.");
    }
    finishVisibleUpdate(window);
    const qint64 interactiveNs = launchTimer.nsecsElapsed();
    QJsonObject result{{QStringLiteral("schema"), 1},
                       {QStringLiteral("platform"), platform},
                       {QStringLiteral("native_surface_exposed"), true},
                       {QStringLiteral("main_entry_to_interactive_ns"), interactiveNs},
                       {QStringLiteral("startup_samples_this_process"), 1},
                       {QStringLiteral("startup_scope"), QStringLiteral("main entry before QApplication to exposed, enabled, painted startup preview; excludes OS process loader")},
                       {QStringLiteral("cache"), QStringLiteral("three warmups per operation; freshly written fixture then warm OS file cache; no cache flush")}};
    // A launcher can timestamp this explicit readiness marker independently.
    writePerformanceJson(directory.filePath(QStringLiteral("performance-ready.json")), result);

    bool idleValid = true;
    const int idleMs = qEnvironmentVariableIsEmpty("YACHT_PERF_IDLE_MS") ? 0
        : qEnvironmentVariableIntValue("YACHT_PERF_IDLE_MS", &idleValid);
    if (!idleValid || idleMs < 0 || idleMs > 60000) {
        throw std::runtime_error("YACHT_PERF_IDLE_MS must be between 0 and 60000.");
    }
    result.insert(QStringLiteral("idle_ms"), idleMs);
    writePerformanceJson(directory.filePath(QStringLiteral("idle-ready.json")), {
        {QStringLiteral("pid"), QCoreApplication::applicationPid()},
        {QStringLiteral("idle_ms"), idleMs},
        {QStringLiteral("instrumentation_polling"), false}});
    if (idleMs > 0) {
        QEventLoop idleLoop;
        QTimer::singleShot(idleMs, &idleLoop, &QEventLoop::quit);
        idleLoop.exec();
    }

    QStringList inputs;
    QStringList markers;
    for (const QString &suffix : {QStringLiteral("A"), QStringLiteral("B")}) {
        const QString marker = QStringLiteral("YACHT_PERF_MARKER_") + suffix;
        const QString input = directory.filePath(QStringLiteral("performance-") + suffix + QStringLiteral(".csv"));
        QFile fixture(input);
        const QByteArray bytes = QByteArray("Label,Count,Notes\n") + marker.toUtf8() +
                                 QByteArray(",123456,Café 🛥\n") + QByteArray("row,123456,Café 🛥\n").repeated(9999);
        if (!fixture.open(QIODevice::WriteOnly) || fixture.write(bytes) != bytes.size()) {
            throw std::runtime_error("Cannot create GUI performance fixture.");
        }
        fixture.close();
        inputs.append(input);
        markers.append(marker);
    }
    result.insert(QStringLiteral("fixture"), QJsonObject{
        {QStringLiteral("body_rows"), 10000}, {QStringLiteral("columns"), 3},
        {QStringLiteral("definition"), QStringLiteral("UTF-8 LF; Label,Count,Notes; unique first-row marker then 9999 row,123456,Café 🛥 records")},
        {QStringLiteral("preview_rows_setting"), window.preferences().previewRows},
        {QStringLiteral("output_mode"), window.preferences().outputMode}});
    int next = 0;
    QString activeMarker;
    QJsonArray measurements;
    measurements.append(measureVisibleOperation(QStringLiteral("loadFile_to_visible_preview"), samples, [&] {
        const int current = next++ % inputs.size();
        activeMarker = markers[current];
        window.loadFile(inputs[current]);
        finishVisibleUpdate(window, activeMarker);
    }));
    measurements.append(measureVisibleOperation(QStringLiteral("refresh_to_visible_preview"), samples, [&] {
        window.onRefresh();
        finishVisibleUpdate(window, activeMarker);
    }));
    measurements.append(measureVisibleOperation(QStringLiteral("render_current_table_to_visible_preview"), samples, [&] {
        window.onPerformRender();
        finishVisibleUpdate(window, activeMarker);
    }));
    measurements.append(measureVisibleOperation(QStringLiteral("source_preview_tab_roundtrip"), samples, [&] {
        window.switchTab(1);
        waitUntil([&] { return window.sourceEdit()->isVisible() && window.sourceEdit()->isEnabled(); });
        window.sourceEdit()->repaint();
        window.switchTab(0);
        finishVisibleUpdate(window, activeMarker);
    }));
    result.insert(QStringLiteral("measurements"), measurements);
    // Preserve completed timings if a later functional check fails.
    QJsonObject functional{{QStringLiteral("status"), QStringLiteral("not_completed")}};
    result.insert(QStringLiteral("functional_checks"), functional);
    writePerformanceJson(directory.filePath(QStringLiteral("performance.json")), result);

    auto *fontField = window.findField(QStringLiteral("font_size_px"));
    if (!fontField) throw std::runtime_error("Font size field is unavailable.");
    fontField->setFieldText(QStringLiteral("14"));
    window.onPerformRender();
    finishVisibleUpdate(window, activeMarker);
    for (int i = 0; i < 50; ++i) {
        fontField->setFieldText(QString::number(10 + i % 10));
    }
    waitUntil([&] {
        return !window.isBusy() &&
               !window.sourceEdit()->document()->find(QStringLiteral("font-size: 19px")).isNull();
    });
    finishVisibleUpdate(window, activeMarker);
    functional.insert(QStringLiteral("rapid_style_edits"), QJsonObject{
        {QStringLiteral("status"), QStringLiteral("passed")},
        {QStringLiteral("edits"), 50}, {QStringLiteral("final_font_size_px"), 19}});
    result.insert(QStringLiteral("functional_checks"), functional);
    writePerformanceJson(directory.filePath(QStringLiteral("performance.json")), result);

    const QString previousSummary = window.summaryText();
    const QString cancelPath = directory.filePath(QStringLiteral("performance-cancel.csv"));
    QFile cancelFixture(cancelPath);
    const QByteArray cancelBytes = QByteArray("A,B\n") + QByteArray("1,2\n").repeated(1000000);
    if (!cancelFixture.open(QIODevice::WriteOnly) || cancelFixture.write(cancelBytes) != cancelBytes.size()) {
        throw std::runtime_error("Cannot create cancellation fixture.");
    }
    cancelFixture.close();
    bool cancelledWhileBusy = false;
    QTimer cancelTimer;
    cancelTimer.setSingleShot(true);
    QObject::connect(&cancelTimer, &QTimer::timeout, &window, [&] {
        cancelledWhileBusy = window.isBusy();
        window.cancelActiveWork();
    });
    cancelTimer.start(5);
    window.loadFile(cancelPath);
    cancelTimer.stop();
    if (!cancelledWhileBusy || window.isBusy() || window.statusText() != QStringLiteral("Cancelled") ||
        window.summaryText() != previousSummary) {
        throw std::runtime_error("In-flight GUI cancellation failed to preserve the previous table and release busy state.");
    }
    window.loadFile(inputs[(next - 1) % inputs.size()]);
    finishVisibleUpdate(window, activeMarker);
    functional.insert(QStringLiteral("cancel_read_and_retry"), QJsonObject{
        {QStringLiteral("status"), QStringLiteral("passed")},
        {QStringLiteral("fixture_body_rows"), 1000000}, {QStringLiteral("timer_delay_ms"), 5},
        {QStringLiteral("cancelled_while_busy"), true}, {QStringLiteral("previous_summary_preserved"), true}});
    functional.insert(QStringLiteral("status"), QStringLiteral("passed"));
    result.insert(QStringLiteral("functional_checks"), functional);
    writePerformanceJson(directory.filePath(QStringLiteral("performance.json")), result);
}

} // namespace

int SmokeTest::run(MainWindow &window, const QString &directory, const QElapsedTimer *launchTimer) {
    if (qEnvironmentVariableIsEmpty("YACHT_TEST_DATA")) {
        qCritical("UI tests require isolated YACHT_TEST_DATA.");
        return 1;
    }

    QDir dir(directory);
    dir.mkpath(QStringLiteral("."));

    try {
        if (launchTimer) recordVisiblePerformance(window, dir, *launchTimer);
        QString input = dir.filePath(QStringLiteral("Café.csv"));
        QFile inputFile(input);
        if (!inputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            throw std::runtime_error("Cannot create input test CSV file.");
        }
        inputFile.write("A,B\n<script>,🛥\n1,2,3\n");
        inputFile.close();

        window.loadFile(input);
        for (const QString &name : {QStringLiteral("N-Blue"), QStringLiteral("N-Gray")}) {
            window.selectPreset(name);
            window.loadSelectedPreset();
            window.onPerformRender();
            auto style = window.currentStyle();
            auto *document = window.previewWidget()->findChild<QTextBrowser *>()->document();
            QTextTable *table = nullptr;
            for (auto it = document->rootFrame()->begin(); !it.atEnd(); ++it) {
                table = qobject_cast<QTextTable *>(it.currentFrame());
                if (table) break;
            }
            QString secondBackground = style.value(style.value(QStringLiteral("zebra_enabled")).toBool()
                                                       ? QStringLiteral("zebra_bg") : QStringLiteral("body_bg")).toString();
            if (!table || table->rows() != 3 || table->columns() < 2 ||
                table->cellAt(0, 0).format().background().color() != QColor(style.value(QStringLiteral("header_bg")).toString()) ||
                table->cellAt(1, 0).format().background().color() != QColor(style.value(QStringLiteral("first_column_bg")).toString()) ||
                table->cellAt(1, 1).format().background().color() != QColor(style.value(QStringLiteral("body_bg")).toString()) ||
                table->cellAt(2, 1).format().background().color() != QColor(secondBackground)) {
                throw std::runtime_error("Native preview table colors do not match " + name.toStdString());
            }
        }
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
        batchArgs[QStringLiteral("output_mode")] = window.preferences().outputMode;
        QJsonArray batchResults = YachtCore::call(QStringLiteral("batch"), batchArgs).toArray();
        if (batchResults.isEmpty() ||
            (batchResults.first().toObject().contains(QStringLiteral("error")) &&
             !batchResults.first().toObject().value(QStringLiteral("error")).isNull())) {
            throw std::runtime_error("Native batch");
        }

        Preferences prefs = Preferences::load();
        prefs.previewRows = 50;
        prefs.save();
        if (Preferences::load().previewRows != 50 || window.styleFields().size() != 18) {
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
