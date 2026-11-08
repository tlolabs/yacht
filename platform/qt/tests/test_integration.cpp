#include "../src/AppIdentity.h"
#include "../src/BatchDialog.h"
#include "../src/DesktopServices.h"
#include "../src/MainWindow.h"
#include "../src/Preferences.h"
#include "../src/Presets.h"
#include "../src/SettingsDialog.h"
#include "../src/SingleInstance.h"
#include "../src/UpdateClient.h"
#include "../src/YachtCore.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QStyle>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextTable>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimer>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using namespace Yacht;

void check(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "Assertion failed: " << message << std::endl;
        std::exit(1);
    }
}

QTextTable *renderedTable(QTextDocument *document) {
    for (auto it = document->rootFrame()->begin(); !it.atEnd(); ++it) {
        if (auto *table = qobject_cast<QTextTable *>(it.currentFrame())) return table;
    }
    return nullptr;
}

void checkRenderedColors(QTextTable *table, const QJsonObject &style, const QString &name) {
    check(table && table->rows() >= 4 && table->columns() >= 2,
          "Rendered preview contains sample cells: " + name.toStdString());
    check(table->cellAt(0, 0).format().background().color() == QColor(style.value(QStringLiteral("header_bg")).toString()),
          "Rendered header color: " + name.toStdString());
    for (int row = 1; row <= 3; ++row) {
        for (int column = 0; column <= 1; ++column) {
            QString key = column == 0 && style.value(QStringLiteral("first_column_enabled")).toBool()
                              ? QStringLiteral("first_column_bg")
                          : row % 2 == 0 && style.value(QStringLiteral("zebra_enabled")).toBool()
                              ? QStringLiteral("zebra_bg")
                              : QStringLiteral("body_bg");
            check(table->cellAt(row, column).format().background().color() == QColor(style.value(key).toString()),
                  "Rendered body color: " + name.toStdString() + " row=" + std::to_string(row) + " column=" + std::to_string(column));
        }
    }
}

class TestDesktopServices : public IDesktopServices {
public:
    bool accept{true};
    QString clipboardText;
    QStringList messages;
    std::function<QStringList()> openFilesFunc;

    QStringList openFiles() override {
        return openFilesFunc ? openFilesFunc() : QStringList();
    }
    std::function<QString(const QString &)> saveFileFunc;
    QString saveFile(const QString &defaultName) override {
        return saveFileFunc ? saveFileFunc(defaultName) : QString();
    }
    bool confirm(const QString &, const QString &, const QString &) override {
        return accept;
    }
    void showMessage(const QString &, const QString &message) override {
        messages.append(message);
    }
    std::optional<bool> reviewBatch(const QStringList &) override {
        return false;
    }
    std::function<bool(Preferences &)> settingsFunc;
    bool settings(Preferences &prefs) override { return settingsFunc ? settingsFunc(prefs) : false; }
    void setClipboard(const QString &text) override { clipboardText = text; }
    QString clipboard() const override { return clipboardText; }
    void reveal(const QString &) override {}
    void open(const QString &) override {}
    QString pickColor(const QString &, const QString &) override { return QString(); }
    void applyAppearance(const QString &) override {}
};

void testRustBinding() {
    QTemporaryDir tempDir;
    check(tempDir.isValid(), "Temporary directory creation");
    QString dir = tempDir.path();

    QString path = dir + QStringLiteral("/Café 🛥.csv");
    QFile file(path);
    check(file.open(QIODevice::WriteOnly | QIODevice::Text), "Open test CSV file");
    file.write("A,B\n<script>,🛥\n1,2,3\n");
    file.close();

    TablePtr table = YachtCore::read(path, QStringLiteral("comma"));
    QJsonObject style = YachtCore::style();
    check(table->metadata().value(QStringLiteral("row_count")).toInt() == 2, "Table row count");

    QJsonObject prevArgs;
    prevArgs[QStringLiteral("style")] = style;
    prevArgs[QStringLiteral("limit")] = 1;
    QJsonObject prev = table->call(QStringLiteral("preview"), prevArgs).toObject();
    check(prev.value(QStringLiteral("html")).toString().contains(QStringLiteral("&lt;script&gt;")),
          "Preview escaping");
    check(prev.value(QStringLiteral("row_count")).toInt() == 1, "Preview row count limit");
    check(prev.value(QStringLiteral("html")).toString().contains(QStringLiteral("<!doctype html>")),
          "Preview renderer always receives a complete document");
    check(prev.value(QStringLiteral("source")).toString().trimmed().startsWith(QStringLiteral("<table")),
          "Default source contains only table markup");

    QString output = dir + QStringLiteral("/out.html");
    QJsonObject expArgs;
    expArgs[QStringLiteral("style")] = style;
    expArgs[QStringLiteral("path")] = output;
    table->call(QStringLiteral("export"), expArgs);

    QFile outFile(output);
    check(outFile.open(QIODevice::ReadOnly), "Read exported HTML");
    QString exportedText = QString::fromUtf8(outFile.readAll());
    outFile.close();

    QJsonObject htmlArgs;
    htmlArgs[QStringLiteral("style")] = style;
    QString htmlDoc = table->call(QStringLiteral("html"), htmlArgs).toString();
    check(exportedText == htmlDoc, "Export matches html call");
    check(htmlDoc.trimmed().startsWith(QStringLiteral("<table")) && !htmlDoc.contains(QStringLiteral("<html")),
          "Default HTML and export contain only the table");

    // Prevent overwrite without overwrite flag
    bool overwriteThrew = false;
    try {
        table->call(QStringLiteral("export"), expArgs);
    } catch (const YachtCoreException &) {
        overwriteThrew = true;
    }
    check(overwriteThrew, "Export overwrite rejection");

    // Cancellation flag test
    bool cancelThrew = false;
    try {
        QJsonObject expArgsOw = expArgs;
        expArgsOw[QStringLiteral("overwrite")] = true;
        table->call(QStringLiteral("export"), expArgsOw, [] { return true; });
    } catch (const YachtCancelledException &) {
        cancelThrew = true;
    }
    check(cancelThrew, "Cancellation callback aborts operation");

    // Presets test
    QJsonObject presetStyle = YachtCore::style(QStringLiteral("unstyled"));
    QJsonObject presetsObj;
    presetsObj[QStringLiteral("Teaching")] = presetStyle;
    QString presetPath = dir + QStringLiteral("/presets.json");

    QJsonObject saveArgs;
    saveArgs[QStringLiteral("path")] = presetPath;
    saveArgs[QStringLiteral("presets")] = presetsObj;
    YachtCore::call(QStringLiteral("save_presets"), saveArgs);

    QJsonObject loadArgs;
    loadArgs[QStringLiteral("path")] = presetPath;
    QJsonObject loaded = YachtCore::call(QStringLiteral("load_presets"), loadArgs).toObject();
    check(loaded == presetsObj, "Preset save and load roundtrip");

    // Batch test
    QJsonObject batchArgs;
    QJsonArray batchInputs;
    batchInputs.append(dir + QStringLiteral("/missing.csv"));
    batchInputs.append(path);
    batchArgs[QStringLiteral("inputs")] = batchInputs;
    batchArgs[QStringLiteral("style")] = style;
    QJsonArray batchResult = YachtCore::call(QStringLiteral("batch"), batchArgs).toArray();
    check(!batchResult[0].toObject().value(QStringLiteral("error")).isNull(),
          "Batch missing file reports error");
    check(batchResult[1].toObject().value(QStringLiteral("error")).isNull(),
          "Batch valid file succeeds");

    // Settings test
    QJsonObject setArgs;
    QJsonObject subSettings;
    subSettings[QStringLiteral("preview_rows")] = 50;
    setArgs[QStringLiteral("settings")] = subSettings;
    QJsonObject setResp = YachtCore::call(QStringLiteral("settings"), setArgs).toObject();
    check(setResp.value(QStringLiteral("settings")).toObject().value(QStringLiteral("preview_rows")).toInt() == 50,
          "Settings call roundtrip");

    // 20 concurrent preview calls
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 20; ++i) {
        futures.push_back(std::async(std::launch::async, [table, style] {
            QJsonObject a;
            a[QStringLiteral("style")] = style;
            table->call(QStringLiteral("preview"), a);
        }));
    }
    for (auto &f : futures) {
        f.get();
    }

    std::cout << "C++ Rust binding: read, preview, export, presets, settings, batch, cancellation, concurrent requests passed." << std::endl;
}

void testViewModelAndPresentation() {
    QTemporaryDir tempDir;
    check(tempDir.isValid(), "Temporary directory creation");
    QString dataDir = tempDir.path();
    qputenv("YACHT_TEST_DATA", dataDir.toUtf8());

    auto desktop = std::make_shared<TestDesktopServices>();
    MainWindow window(desktop);
    window.initialize();

    check(window.styleFields().size() == 18, "All 18 style fields initialized");
    check(window.sourceEdit()->toPlainText().contains(QStringLiteral("<table")), "Sample source contains table");

    QString input = dataDir + QStringLiteral("/Unicode 🛥.csv");
    QFile f(input);
    check(f.open(QIODevice::WriteOnly | QIODevice::Text), "Write Unicode CSV");
    f.write("A,B\n<script>,🛥\n1,2,3\n");
    f.close();

    window.loadFile(input);
    check(window.summaryText().contains(QStringLiteral("2 rows")), "Open rows/warnings in summary");

    window.onResetUnstyled();
    window.onPerformRender();
    check(!window.sourceEdit()->toPlainText().contains(QStringLiteral("<style>")), "Unstyled has no <style>");
    check(window.sourceEdit()->toPlainText().contains(QStringLiteral("&lt;script&gt;")), "Markup escaped");

    window.copyHtmlToClipboard();
    check(desktop->clipboardText.contains(QStringLiteral("&lt;script&gt;")), "Clipboard contains escaped HTML");

    QString output = dataDir + QStringLiteral("/out.html");
    window.exportTo(output, false);
    QFile outFile(output);
    check(outFile.open(QIODevice::ReadOnly), "Read exported HTML file");
    QString outContent = QString::fromUtf8(outFile.readAll());
    outFile.close();
    check(outContent == desktop->clipboardText, "Export content matches clipboard HTML");

    // Overwrite without flag throws
    bool overwriteThrew = false;
    try {
        window.exportTo(output, false);
    } catch (...) {
        overwriteThrew = true;
    }
    check(overwriteThrew, "Export overwrite protection");
    check(!window.isBusy(), "Failure releases busy state");

    // Presets
    window.setPresetNameInput(QStringLiteral("Teaching"));
    window.saveCurrentPreset();
    check(window.presetNames().contains(QStringLiteral("Teaching")), "Saved preset appears in names");

    window.onResetStyled();
    window.selectPreset(QStringLiteral("Teaching"));
    window.loadSelectedPreset();
    window.onPerformRender();
    check(!window.sourceEdit()->toPlainText().contains(QStringLiteral("<style>")), "Loaded preset has unstyled style");

    window.selectPreset(QStringLiteral("Teaching"));
    desktop->accept = false;
    window.deleteSelectedPreset();
    check(window.presetNames().contains(QStringLiteral("Teaching")), "Delete cancellation preserved");

    desktop->accept = true;
    window.deleteSelectedPreset();
    check(!window.presetNames().contains(QStringLiteral("Teaching")), "Deleted preset removed from names");

    // Batch Convert
    desktop->messages.clear();
    // Simulate batch via Rust core batch
    QJsonObject batchStyle = window.currentStyle();
    QJsonObject batchArgs;
    QJsonArray batchInputs;
    batchInputs.append(dataDir + QStringLiteral("/missing.csv"));
    batchInputs.append(input);
    batchArgs[QStringLiteral("inputs")] = batchInputs;
    batchArgs[QStringLiteral("style")] = batchStyle;
    QJsonArray batchRes = YachtCore::call(QStringLiteral("batch"), batchArgs).toArray();
    check(batchRes.size() == 2, "Batch processed 2 files");
    check(!batchRes[1].toObject().value(QStringLiteral("output")).toString().isEmpty(), "Batch second file saved");

    // Recent files
    check(window.preferences().recent.contains(input), "Recent file list in memory");
    check(Preferences::load().recent.contains(input), "Saved recent files on disk");

    // Style validation failure and recovery
    StyleField *fontField = window.findField(QStringLiteral("font_size_px"));
    check(fontField != nullptr, "Find font_size_px field");
    fontField->setFieldText(QStringLiteral("not-a-number"));
    window.onPerformRender();
    check(window.sourceEdit()->toPlainText().isEmpty(), "Invalid style clears stale preview");

    fontField->setFieldText(QStringLiteral("20"));
    window.onPerformRender();
    check(window.sourceEdit()->toPlainText().contains(QStringLiteral("<table")), "Recovery after validation failure");

    // TSV inference
    QString tsvPath = dataDir + QStringLiteral("/tabs.tsv");
    QFile tsvFile(tsvPath);
    check(tsvFile.open(QIODevice::WriteOnly | QIODevice::Text), "Write TSV file");
    tsvFile.write("A\tB\n1\t2\n");
    tsvFile.close();

    window.loadFile(tsvPath);
    check(window.delimiter() == QStringLiteral("tab"), "TSV inference selects tab delimiter");

    // Internal updater isolation
    if (AppIdentity::isInternal()) {
        window.checkUpdates(false);
        check(desktop->messages.last().contains(QStringLiteral("Production updates are disabled")),
              "Internal updater isolation");
    }

    // Linux preferences import
    QString legacyIni = dataDir + QStringLiteral("/ui.ini");
    QFile iniFile(legacyIni);
    check(iniFile.open(QIODevice::WriteOnly | QIODevice::Text), "Write legacy ui.ini");
    iniFile.write("[ui]\nremember_style=false\npreview_rows=50\nappearance=Dark\nrecent_files=[\"/tmp/Café.csv\"]\nlast_style={\"font_size_px\":18}\n");
    iniFile.close();

    Preferences migrated = Preferences::importLinuxPreferences(legacyIni);
    check(!migrated.rememberStyle && migrated.previewRows == 50 && migrated.appearance == QStringLiteral("Dark"),
          "Linux preferences flags migration");
    check(migrated.recent.size() == 1 && migrated.recent.first() == QStringLiteral("/tmp/Café.csv"),
          "Linux recent files migration");
    check(migrated.lastStyle.value(QStringLiteral("font_size_px")).toInt() == 18,
          "Linux last style migration");
    check(migrated.outputMode == QStringLiteral("table"), "Legacy Linux preferences default to table HTML");
    check(QFile::exists(legacyIni), "Legacy ini preserved");

    // Huge CSV cancellation
    QString hugePath = dataDir + QStringLiteral("/cancel.csv");
    QFile hugeFile(hugePath);
    check(hugeFile.open(QIODevice::WriteOnly | QIODevice::Text), "Write huge CSV file");
    QByteArray bigData;
    bigData.reserve(5000000);
    bigData.append("A,B\n");
    for (int i = 0; i < 1000000; ++i) {
        bigData.append("1,2\n");
    }
    hugeFile.write(bigData);
    hugeFile.close();

    window.cancelActiveWork();
    window.loadFile(hugePath);
    check(!window.isBusy(), "Cancellation releases busy state");

    // Cancel after a worker has started. This catches a GUI-thread read that
    // prevents the Cancel button and keyboard shortcut from receiving events.
    bool cancelledWhileBusy = false;
    QTimer::singleShot(5, &window, [&] {
        cancelledWhileBusy = window.isBusy();
        window.cancelActiveWork();
    });
    window.loadFile(hugePath);
    check(cancelledWhileBusy, "GUI event loop remains responsive during read");
    check(window.statusText() == QStringLiteral("Cancelled"), "In-flight read was cancelled");
    check(window.summaryText().contains(QStringLiteral("tabs.tsv")), "Cancelled read preserves the previous table");

    window.loadFile(tsvPath);
    check(window.sourceEdit()->toPlainText().contains(QStringLiteral("<table")), "Retry after cancellation");

    // Corrupted preferences preservation
    QString prefJsonPath = dataDir + QStringLiteral("/ui.json");
    QFile prefFile(prefJsonPath);
    check(prefFile.open(QIODevice::ReadOnly), "Read valid preferences");
    QByteArray validPrefs = prefFile.readAll();
    prefFile.close();

    check(prefFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Corrupt preferences");
    prefFile.write("{ broken existing preferences");
    prefFile.close();

    {
        MainWindow recovery(desktop);
        recovery.initialize();
        recovery.savePreferences();
    }

    check(prefFile.open(QIODevice::ReadOnly), "Verify broken preferences preserved");
    QByteArray preserved = prefFile.readAll();
    prefFile.close();
    check(preserved == "{ broken existing preferences", "Unreadable preferences are not overwritten");

    // Restore
    check(prefFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Restore preferences");
    prefFile.write(validPrefs);
    prefFile.close();

    // -------------------------------------------------------------------------
    // QLT-01 Regression: Preset store data preservation & recovery
    // -------------------------------------------------------------------------
    QString presetFilePath = AppIdentity::presetPath();
    QFile presetFile(presetFilePath);
    check(presetFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Corrupt preset store");
    QByteArray corruptedPresetData = "{ invalid json preset store: ";
    presetFile.write(corruptedPresetData);
    presetFile.close();

    {
        MainWindow presetRecovery(desktop);
        presetRecovery.initialize();
        check(!presetRecovery.presetsReadable(), "Corrupted preset store marked unreadable");

        // Attempting to save preset without explicit recovery is refused and preserves original bytes
        desktop->accept = false;
        presetRecovery.setPresetNameInput(QStringLiteral("ShouldNotOverwrite"));
        presetRecovery.saveCurrentPreset();

        // Attempting to delete preset is also refused
        presetRecovery.deleteSelectedPreset();
    }

    check(presetFile.open(QIODevice::ReadOnly), "Verify unreadable presets preserved");
    QByteArray preservedPresets = presetFile.readAll();
    presetFile.close();
    check(preservedPresets == corruptedPresetData, "Exact original bytes of corrupt preset file preserved");

    // Invalid remembered style does NOT prevent loading valid presets
    check(presetFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Write valid preset store");
    presetFile.write("{\"CustomPreset\":{\"font_size_px\":24}}");
    presetFile.close();

    check(prefFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Write preferences with invalid last_style");
    prefFile.write("{\"remember_style\":true,\"last_style\":{\"font_size_px\":\"not-a-number\"}}");
    prefFile.close();

    {
        MainWindow sepWindow(desktop);
        sepWindow.initialize();
        check(sepWindow.presetsReadable(), "Presets loaded despite invalid last_style");
        check(sepWindow.presetNames().contains(QStringLiteral("CustomPreset")), "Valid preset loaded when lastStyle is invalid");
    }

    // Explicit recovery behavior with backup
    check(presetFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Re-corrupt preset store for recovery test");
    presetFile.write(corruptedPresetData);
    presetFile.close();

    {
        MainWindow recWindow(desktop);
        recWindow.initialize();
        check(!recWindow.presetsReadable(), "Store is unreadable before recovery");
        bool recovered = recWindow.recoverPresetsWithBackup();
        check(recovered, "Explicit recovery succeeded");
        check(recWindow.presetsReadable(), "Store is readable after recovery");
        recWindow.setPresetNameInput(QStringLiteral("FreshPreset"));
        recWindow.saveCurrentPreset();
        check(recWindow.presetNames().contains(QStringLiteral("FreshPreset")), "Can save preset after explicit recovery");
    }

    // -------------------------------------------------------------------------
    // QLT-02 Regression: Qt GUI callback exception containment
    // -------------------------------------------------------------------------
    {
        MainWindow guiWindow(desktop);
        guiWindow.initialize();

        // 1. Unwritable export destination in onExport()
        desktop->saveFileFunc = [](const QString &) {
            return QStringLiteral("/nonexistent/directory/path/export.html");
        };
        desktop->messages.clear();
        guiWindow.onExport();
        check(!guiWindow.isBusy(), "onExport to unwritable path resets busy state");
        check(!desktop->messages.isEmpty(), "onExport displays error dialog on write failure");

        // 2. Invalid style validation in onExport()
        desktop->saveFileFunc = [&](const QString &) {
            return dataDir + QStringLiteral("/export_invalid.html");
        };
        StyleField *sizeField = guiWindow.findField(QStringLiteral("font_size_px"));
        check(sizeField != nullptr, "Find font_size_px field");
        sizeField->setFieldText(QStringLiteral("invalid-size"));
        desktop->messages.clear();
        guiWindow.onExport();
        check(!guiWindow.isBusy(), "onExport with invalid style resets busy state");
        check(!desktop->messages.isEmpty(), "onExport with invalid style displays error message");

        // 3. Batch conversion with invalid style
        desktop->messages.clear();
        guiWindow.batchConvert({dataDir + QStringLiteral("/tabs.tsv")});
        check(!guiWindow.isBusy(), "batchConvert with invalid style resets busy state");
        check(!desktop->messages.isEmpty(), "batchConvert with invalid style displays error message");

        // Reset field to valid
        sizeField->setFieldText(QStringLiteral("14"));
    }

    qunsetenv("YACHT_TEST_DATA");
    std::cout << "Shared Qt presentation: all assertions passed." << std::endl;
}

void testOutputModesAndBuiltins() {
    QTemporaryDir tempDir;
    check(tempDir.isValid(), "Output modes temporary directory");
    qputenv("YACHT_TEST_DATA", tempDir.path().toUtf8());

    {
        check(Preferences::load().outputMode == QStringLiteral("table"), "New preferences default to table HTML");
        QFile preferenceFile(tempDir.path() + QStringLiteral("/ui.json"));
        check(preferenceFile.open(QIODevice::WriteOnly), "Write older preferences");
        preferenceFile.write("{\"RememberStyle\":true,\"PreviewRows\":50}");
        preferenceFile.close();
        check(Preferences::load().outputMode == QStringLiteral("table"), "Older JSON preferences default to table HTML");

        check(preferenceFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Write invalid HTML mode preference");
        preferenceFile.write("{\"output_mode\":\"unknown\"}");
        preferenceFile.close();
        check(Preferences::load().outputMode == QStringLiteral("table"), "Invalid HTML mode falls back to table HTML");

        QJsonObject custom;
        custom[QStringLiteral("N-Blue")] = YachtCore::style(QStringLiteral("unstyled"));
        custom[QStringLiteral("G-Cyan")] = YachtCore::style(QStringLiteral("unstyled"));
        Presets::save(custom);

        auto desktop = std::make_shared<TestDesktopServices>();
        MainWindow window(desktop);
        window.initialize();
        QJsonObject builtins = YachtCore::call(QStringLiteral("builtin_presets")).toObject();
        check(builtins.size() == 19, "Nineteen built-in presets available");
        check(window.presetNames().size() == builtins.size() + custom.size(), "Preset picker includes every shared built-in and custom preset");
        const QStringList googleNames{
            QStringLiteral("G-Gray"), QStringLiteral("G-Cyan"), QStringLiteral("G-Green"),
            QStringLiteral("G-Yellow"), QStringLiteral("G-Orange"), QStringLiteral("G-Blue"),
            QStringLiteral("G-Teal"), QStringLiteral("G-Blue Gray"), QStringLiteral("G-Brown"),
            QStringLiteral("G-Light Green"), QStringLiteral("G-Purple")};
        for (const auto &name : googleNames) {
            check(builtins.contains(name), "Google built-in preset is available: " + name.toStdString());
            auto style = builtins.value(name).toObject();
            check(style.value(QStringLiteral("zebra_enabled")).toBool(), "Google preset alternates row colors: " + name.toStdString());
            check(!style.value(QStringLiteral("first_column_enabled")).toBool(), "Google preset colors the whole row: " + name.toStdString());
        }
        check(!window.presetNames().contains(QStringLiteral("Default (Styled)")), "Original built-in is renamed");
        check(window.presetNames().contains(QStringLiteral("Y-Default")), "Original style remains as Y-Default");
        check(window.presetNames().contains(QStringLiteral("N-Blue (Custom)")), "Existing custom name collision is preserved");
        check(window.presetNames().contains(QStringLiteral("G-Cyan (Custom)")), "Existing custom Google preset name collision is preserved");
        window.selectPreset(QStringLiteral("G-Cyan (Custom)"));
        window.loadSelectedPreset();
        check(window.currentStyle() == YachtCore::style(QStringLiteral("unstyled")), "Custom preset with Google built-in name remains loadable");
        window.selectPreset(QStringLiteral("N-Blue (Custom)"));
        window.loadSelectedPreset();
        check(window.currentStyle() == YachtCore::style(QStringLiteral("unstyled")), "Custom preset with new built-in name remains loadable");
        window.setStyle(builtins.value(QStringLiteral("Y-Default")).toObject());
        window.setPresetNameInput(QStringLiteral("N-Blue"));
        desktop->accept = false;
        window.saveCurrentPreset();
        check(Presets::load().value(QStringLiteral("N-Blue")).toObject() == YachtCore::style(QStringLiteral("unstyled")),
              "Replacing an existing custom collision requires confirmation");
        desktop->accept = true;
        window.saveCurrentPreset();
        check(window.currentPresetName() == QStringLiteral("N-Blue (Custom)"), "Saving custom collision selects the custom preset");
        check(Presets::load().value(QStringLiteral("N-Blue")) == builtins.value(QStringLiteral("Y-Default")),
              "Existing custom collision can be replaced");
        window.loadSelectedPreset();
        check(window.currentStyle() == builtins.value(QStringLiteral("Y-Default")).toObject(), "Updated custom collision remains loadable");
        window.selectPreset(QStringLiteral("N-Blue"));
        window.loadSelectedPreset();
        check(window.currentStyle() == builtins.value(QStringLiteral("N-Blue")).toObject(), "Replacing custom collision preserves the built-in");

        for (auto it = builtins.constBegin(); it != builtins.constEnd(); ++it) {
            check(window.presetNames().contains(it.key()), "Built-in preset appears: " + it.key().toStdString());
            window.selectPreset(it.key());
            window.loadSelectedPreset();
            check(window.currentStyle() == it.value().toObject(), "Built-in style applied: " + it.key().toStdString());
            window.onPerformRender();
            if (it.key() != QStringLiteral("Unstyled")) {
                checkRenderedColors(renderedTable(window.previewWidget()->findChild<QTextBrowser *>()->document()),
                                    it.value().toObject(), it.key());
            }
            window.deleteSelectedPreset();
            check(window.presetNames().contains(it.key()), "Built-in preset cannot be deleted");
        }
        window.selectPreset(QStringLiteral("N-Gray"));
        window.loadSelectedPreset();
        check(window.currentStyle().value(QStringLiteral("first_column_enabled")).toBool(), "N-Gray fills the first column");
        window.setStyle(QJsonObject{{QStringLiteral("font_size_px"), 18}});
        check(!window.currentStyle().value(QStringLiteral("first_column_enabled")).toBool(),
              "Legacy partial style does not inherit first-column fill");
        window.setPresetNameInput(QStringLiteral("N-Orange"));
        window.saveCurrentPreset();
        check(!desktop->messages.isEmpty() && desktop->messages.last().contains(QStringLiteral("built-in")),
              "Saving over a built-in is rejected");

        window.onResetStyled();
        window.onPerformRender();
        check(window.sourceEdit()->toPlainText().trimmed().startsWith(QStringLiteral("<table")), "Initial source uses table HTML");
        check(!window.previewWidget()->findChild<QTextBrowser *>()->toPlainText().trimmed().isEmpty(), "Table mode has a rendered preview");

        auto sample = YachtCore::sample();
        for (const QString &mode : {QStringLiteral("table"), QStringLiteral("document")}) {
            QJsonObject args{{QStringLiteral("style"), builtins.value(QStringLiteral("N-Gray"))},
                             {QStringLiteral("limit"), 2},
                             {QStringLiteral("inline_preview"), true},
                             {QStringLiteral("output_mode"), mode}};
            auto preview = sample->call(QStringLiteral("preview"), args).toObject();
            QTextDocument document;
            document.setHtml(preview.value(QStringLiteral("html")).toString());
            auto *table = renderedTable(&document);
            check(table && table->rows() == 3, "Inline preview respects row limit including one header row");
            check(preview.value(QStringLiteral("row_count")).toInt() == 2, "Inline preview reports limited row count");
            check(table->cellAt(1, 0).format().background().color() == QColor(QStringLiteral("#dcdcdc")),
                  "Limited inline preview preserves gray first-column fill");
            QString source = preview.value(QStringLiteral("source")).toString();
            check(source.count(QStringLiteral("<tr>")) == sample->metadata().value(QStringLiteral("row_count")).toInt() + 1,
                  "Inline renderer keeps all rows in source");
            check(mode == QStringLiteral("document") ? source.startsWith(QStringLiteral("<!doctype html>")) : source.startsWith(QStringLiteral("<table")),
                  "Inline renderer leaves selected source format independent");
        }

        for (const QString &mode : {QStringLiteral("document"), QStringLiteral("table")}) {
            desktop->settingsFunc = [mode](Preferences &prefs) {
                SettingsDialog dialog(prefs);
                auto *combo = dialog.findChild<QComboBox *>(QStringLiteral("outputModeCombo"));
                check(combo != nullptr, "Settings exposes HTML output choice");
                check(combo->currentData().toString() == prefs.outputMode, "Settings restores HTML output choice");
                combo->setCurrentIndex(combo->findData(mode));
                check(QMetaObject::invokeMethod(&dialog, "onSave", Qt::DirectConnection), "Save HTML output settings");
                return dialog.result() == QDialog::Accepted;
            };
            window.onSettings();
            bool document = mode == QStringLiteral("document");
            check(Preferences::load().outputMode == mode, "HTML output choice persists");
            QString source = window.sourceEdit()->toPlainText().trimmed();
            check(document ? source.startsWith(QStringLiteral("<!doctype html>")) : source.startsWith(QStringLiteral("<table")),
                  "Source follows selected output format");
            window.copyHtmlToClipboard();
            check(document ? desktop->clipboardText.contains(QStringLiteral("<!doctype html>")) : !desktop->clipboardText.contains(QStringLiteral("<html")),
                  "Clipboard follows selected output format");
            QString output = tempDir.path() + QLatin1Char('/') + mode + QStringLiteral(".html");
            window.exportTo(output, false);
            QFile exported(output);
            check(exported.open(QIODevice::ReadOnly), "Read format-specific export");
            check(QString::fromUtf8(exported.readAll()) == desktop->clipboardText, "Export matches selected clipboard format");

            QString csvPath = tempDir.path() + QLatin1Char('/') + mode + QStringLiteral("-batch.csv");
            QFile csv(csvPath);
            check(csv.open(QIODevice::WriteOnly), "Write batch input for output format");
            csv.write("A,B\n1,2\n");
            csv.close();
            window.batchConvert({csvPath});
            QFile batched(tempDir.path() + QLatin1Char('/') + mode + QStringLiteral("-batch.html"));
            check(batched.open(QIODevice::ReadOnly), "Read batch HTML output");
            QString batchHtml = QString::fromUtf8(batched.readAll()).trimmed();
            check(document ? batchHtml.startsWith(QStringLiteral("<!doctype html>")) : batchHtml.startsWith(QStringLiteral("<table")),
                  "Batch conversion follows selected output format");
        }
    }
    qunsetenv("YACHT_TEST_DATA");
    std::cout << "HTML output settings and built-in styles: all assertions passed." << std::endl;
}

void testAccessibilityAndPlatformSupport() {
    auto desktop = std::make_shared<TestDesktopServices>();
    MainWindow window(desktop);
    window.initialize();

    // 1. Verify push buttons have accessible name/text and non-empty tooltips
    QList<QPushButton *> buttons = window.findChildren<QPushButton *>();
    check(!buttons.isEmpty(), "Found buttons in MainWindow");
    for (auto *btn : buttons) {
        QString name = btn->accessibleName().isEmpty() ? btn->text() : btn->accessibleName();
        check(!name.trimmed().isEmpty(), "Button has accessible name or text: " + btn->objectName().toStdString());
        check(!btn->toolTip().trimmed().isEmpty(), "Button has tooltip: " + name.toStdString());
    }

    // 2. Verify combo boxes have accessible name and non-empty tooltips
    QList<QComboBox *> combos = window.findChildren<QComboBox *>();
    check(!combos.isEmpty(), "Found combo boxes in MainWindow");
    for (auto *combo : combos) {
        check(!combo->accessibleName().trimmed().isEmpty(), "Combo box has accessible name");
        check(!combo->toolTip().trimmed().isEmpty(), "Combo box has tooltip");
    }

    // 3. Verify shortcuts on standard actions
    QList<QAction *> actions = window.findChildren<QAction *>();
    bool foundRefresh = false;
    bool foundSettings = false;
    bool foundHelp = false;
    for (auto *act : actions) {
        if (act->text().contains(QStringLiteral("Refresh"), Qt::CaseInsensitive)) {
            check(act->shortcuts().contains(QKeySequence::Refresh) || act->shortcuts().contains(QKeySequence(QStringLiteral("Ctrl+R"))),
                  "Refresh shortcuts contain standard refresh sequence");
            foundRefresh = true;
        } else if (act->text().contains(QStringLiteral("Settings"), Qt::CaseInsensitive)) {
            check(act->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_Comma), "Settings shortcut Ctrl+,");
            foundSettings = true;
        } else if (act->text().contains(QStringLiteral("User Guide"), Qt::CaseInsensitive)) {
            check(act->shortcut() == QKeySequence::HelpContents || act->shortcut() == QKeySequence(Qt::Key_F1),
                  "User Guide standard help shortcut");
            foundHelp = true;
        }
    }
    check(foundRefresh, "Refresh action found");
    check(foundSettings, "Settings action found");
    check(foundHelp, "Help action found");

    // 4. Exercise all three appearance settings and restoration of the OS style.
    QtDesktopServices realServices(nullptr);
    QString systemStyle = qApp->style()->objectName();
    realServices.applyAppearance(QStringLiteral("Dark"));
    QPalette darkPal = qApp->palette();
    check(darkPal.color(QPalette::Window).lightness() < 128, "Dark appearance uses a dark window");
    check(darkPal.color(QPalette::ToolTipBase) != darkPal.color(QPalette::ToolTipText),
          "Dark palette tooltip base and text must have contrast");
    realServices.applyAppearance(QStringLiteral("Light"));
    QPalette lightPal = qApp->palette();
    check(lightPal.color(QPalette::Window).lightness() >= 128, "Light appearance uses a light window");
    check(lightPal.color(QPalette::Text) != lightPal.color(QPalette::Base),
          "Light palette text and base must have contrast");
    realServices.applyAppearance(QStringLiteral("System"));
    check(qApp->style()->objectName() == systemStyle, "System appearance restores the OS style");

    // -------------------------------------------------------------------------
    // A11Y-01 Regression: Preview contrast in Dark, Light, and System appearances
    // -------------------------------------------------------------------------
    HtmlPreviewWidget contrastPreview;
    TablePtr sampleTable = YachtCore::sample();
    QJsonObject previewArgs;
    previewArgs[QStringLiteral("style")] = YachtCore::style(QStringLiteral("defaults"));
    previewArgs[QStringLiteral("limit")] = 10;
    QJsonObject rendered = sampleTable->call(QStringLiteral("preview"), previewArgs).toObject();
    QString previewHtml = rendered.value(QStringLiteral("html")).toString();

    // Verify generated HTML specifies accessible foreground text color in table cells
    check(previewHtml.contains(QStringLiteral("color: #222222")),
          "Preview HTML specifies accessible body cell text color");
    check(previewHtml.contains(QStringLiteral("background: #ffffff")),
          "Preview HTML specifies light body background");
    check(previewHtml.contains(QStringLiteral("background: #fbfbfb")),
          "Preview HTML specifies zebra background");
    check(previewHtml.contains(QStringLiteral("background: #f2f8ff")),
          "Preview HTML specifies hover background");

    // Apply Dark appearance and verify rendering
    realServices.applyAppearance(QStringLiteral("Dark"));
    contrastPreview.setHtml(previewHtml);
    check(!contrastPreview.findChild<QTextBrowser *>()->toPlainText().trimmed().isEmpty(),
          "Preview renders successfully under Dark appearance");

    // Apply Light appearance and verify rendering
    realServices.applyAppearance(QStringLiteral("Light"));
    contrastPreview.setHtml(previewHtml);
    check(!contrastPreview.findChild<QTextBrowser *>()->toPlainText().trimmed().isEmpty(),
          "Preview renders successfully under Light appearance");

    realServices.applyAppearance(QStringLiteral("System"));

    // 5. Verify BatchDialog accessibility
    BatchDialog batchDialog({QStringLiteral("/tmp/sample.csv")});
    auto batchCheckboxes = batchDialog.findChildren<QCheckBox *>();
    check(!batchCheckboxes.isEmpty(), "BatchDialog has checkbox");
    check(!batchCheckboxes.first()->accessibleName().isEmpty(), "BatchDialog checkbox accessible name");
    check(!batchCheckboxes.first()->toolTip().isEmpty(), "BatchDialog checkbox tooltip");

    // 6. Verify SettingsDialog accessibility
    Preferences testPrefs;
    testPrefs.recent.append(QStringLiteral("/tmp/sample.csv"));
    SettingsDialog settingsDialog(testPrefs);
    auto settingsCombos = settingsDialog.findChildren<QComboBox *>();
    check(settingsCombos.size() >= 2, "SettingsDialog has combo boxes");
    for (auto *c : settingsCombos) {
        check(!c->accessibleName().isEmpty(), "Settings combo accessible name");
        check(!c->toolTip().isEmpty(), "Settings combo tooltip");
    }

    std::cout << "Accessibility and platform support: all assertions passed." << std::endl;
}

void testSingleInstanceArgumentHandling() {
    std::cout << "Testing SingleInstance argument handling (QLT-03)..." << std::endl;

    // 1. argv[0] only -> empty list (activation)
    QStringList noFiles{QStringLiteral("/usr/local/bin/yacht")};
    QStringList normalized = SingleInstance::normalizeArguments(noFiles);
    check(normalized.isEmpty(), "No-file invocation normalizes to empty list (activation)");

    // 2. Single CSV file -> 1 file (excludes argv[0])
    QStringList oneFile{QStringLiteral("/usr/local/bin/yacht"), QStringLiteral("data.csv")};
    normalized = SingleInstance::normalizeArguments(oneFile);
    check(normalized.size() == 1 && normalized[0] == QStringLiteral("data.csv"),
          "Single file excludes argv[0] and contains only the CSV");

    // 3. Multiple files with spaces and Unicode
    QStringList multiFiles{QStringLiteral("yacht"), QStringLiteral("path with spaces.csv"),
                           QStringLiteral("Unicode 🛥.tsv")};
    normalized = SingleInstance::normalizeArguments(multiFiles);
    check(normalized.size() == 2 &&
          normalized[0] == QStringLiteral("path with spaces.csv") &&
          normalized[1] == QStringLiteral("Unicode 🛥.tsv"),
          "Multiple files preserve spaces and Unicode characters");

    // 4. Options and smoke test args are excluded
    QStringList smokeArgs{QStringLiteral("yacht"), QStringLiteral("--ui-smoke-test"),
                          QStringLiteral("smoke.json"), QStringLiteral("target.csv")};
    normalized = SingleInstance::normalizeArguments(smokeArgs);
    check(normalized.size() == 1 && normalized[0] == QStringLiteral("target.csv"),
          "Option flags and their arguments are excluded");

    // 5. Dash-prefixed file after '--' is preserved as file
    QStringList dashFiles{QStringLiteral("yacht"), QStringLiteral("--"), QStringLiteral("-my-file.csv")};
    normalized = SingleInstance::normalizeArguments(dashFiles);
    check(normalized.size() == 1 && normalized[0] == QStringLiteral("-my-file.csv"),
          "Dash-prefixed filename after '--' is preserved as a file argument");

    std::cout << "SingleInstance argument handling: all assertions passed." << std::endl;
}

void testSecureUpdaterResolution() {
    std::cout << "Testing secure updater executable resolution (SEC-02)..." << std::endl;

    // Ensure dev overrides are unset
    qunsetenv("YACHT_DEV_UPDATER_PATH");
    qunsetenv("YACHT_ALLOW_DEV_UPDATER");

    // In production without bundled binary, resolution fails closed
    QString resolved = UpdateClient::findUpdaterBinary();
    // In test environment, the bundled binary is not present in build dir
    if (!QFile::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/yacht-update"))) {
        check(resolved.isEmpty(), "Fails closed when bundled updater is absent");
    }

    // Create a fake replacement executable in a separate directory and switch CWD there
    QString originalCwd = QDir::currentPath();
    QTemporaryDir fakeCwdDir;
    check(fakeCwdDir.isValid(), "Created temp dir for CWD test");
    QString fakeCwd = fakeCwdDir.path() + QStringLiteral("/yacht-update");
    QFile fakeCwdFile(fakeCwd);
    if (fakeCwdFile.open(QIODevice::WriteOnly)) {
        fakeCwdFile.write("#!/bin/sh\nexit 1\n");
        fakeCwdFile.close();
        fakeCwdFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

        // Change current directory to fakeCwdDir
        QDir::setCurrent(fakeCwdDir.path());

        // findUpdaterBinary MUST NOT resolve the fake CWD executable in production
        QString cwdResolved = UpdateClient::findUpdaterBinary();
        check(cwdResolved != fakeCwd && cwdResolved.isEmpty(),
              "Never executes replacement helper from current working directory");

        QDir::setCurrent(originalCwd);
    }

    // Test explicit developer override via environment variable
    QString tempDevHelper = QDir::tempPath() + QStringLiteral("/fake-yacht-update");
    QFile devFile(tempDevHelper);
    if (devFile.open(QIODevice::WriteOnly)) {
        devFile.write("#!/bin/sh\nexit 0\n");
        devFile.close();
        devFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

        qputenv("YACHT_DEV_UPDATER_PATH", tempDevHelper.toUtf8());
        QString devResolved = UpdateClient::findUpdaterBinary();
        check(devResolved == tempDevHelper, "Explicit developer override resolves specified helper path");

        qunsetenv("YACHT_DEV_UPDATER_PATH");
        devFile.remove();
    }

    std::cout << "Secure updater executable resolution: all assertions passed." << std::endl;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::cout << "Running Qt Rust binding and integration tests..." << std::endl;
    testRustBinding();
    testViewModelAndPresentation();
    testOutputModesAndBuiltins();
    testAccessibilityAndPlatformSupport();
    testSingleInstanceArgumentHandling();
    testSecureUpdaterResolution();
    std::cout << "All integration tests passed successfully!" << std::endl;
    return 0;
}
