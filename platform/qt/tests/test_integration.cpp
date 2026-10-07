#include "../src/AppIdentity.h"
#include "../src/BatchDialog.h"
#include "../src/DesktopServices.h"
#include "../src/MainWindow.h"
#include "../src/Preferences.h"
#include "../src/Presets.h"
#include "../src/SettingsDialog.h"
#include "../src/YachtCore.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
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

class TestDesktopServices : public IDesktopServices {
public:
    bool accept{true};
    QString clipboardText;
    QStringList messages;
    std::function<QStringList()> openFilesFunc;

    QStringList openFiles() override {
        return openFilesFunc ? openFilesFunc() : QStringList();
    }
    QString saveFile(const QString &) override { return QString(); }
    bool confirm(const QString &, const QString &, const QString &) override {
        return accept;
    }
    void showMessage(const QString &, const QString &message) override {
        messages.append(message);
    }
    std::optional<bool> reviewBatch(const QStringList &) override {
        return false;
    }
    bool settings(Preferences &) override { return false; }
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

    check(window.styleFields().size() == 16, "All 16 style fields initialized");
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
    check(QFile::exists(legacyIni), "Legacy ini preserved");

    // Huge CSV cancellation
    QString hugePath = dataDir + QStringLiteral("/cancel.csv");
    QFile hugeFile(hugePath);
    check(hugeFile.open(QIODevice::WriteOnly | QIODevice::Text), "Write huge CSV file");
    QByteArray bigData;
    bigData.reserve(100000);
    bigData.append("A,B\n");
    for (int i = 0; i < 20000; ++i) {
        bigData.append("1,2\n");
    }
    hugeFile.write(bigData);
    hugeFile.close();

    window.cancelActiveWork();
    window.loadFile(hugePath);
    check(!window.isBusy(), "Cancellation releases busy state");

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

    qunsetenv("YACHT_TEST_DATA");
    std::cout << "Shared Qt presentation: all assertions passed." << std::endl;
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

    // 4. Verify dark appearance palette contrast
    QtDesktopServices realServices(nullptr);
    realServices.applyAppearance(QStringLiteral("Dark"));
    QPalette darkPal = qApp->palette();
    check(darkPal.color(QPalette::ToolTipBase) != darkPal.color(QPalette::ToolTipText),
          "Dark palette tooltip base and text must have contrast");
    check(darkPal.color(QPalette::HighlightedText) == Qt::white,
          "Dark palette highlighted text is white");
    check(darkPal.color(QPalette::Disabled, QPalette::Text) == QColor(128, 128, 128),
          "Dark palette disabled text color");

    // Reset to System
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

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::cout << "Running Qt Rust binding and integration tests..." << std::endl;
    testRustBinding();
    testViewModelAndPresentation();
    testAccessibilityAndPlatformSupport();
    std::cout << "All integration tests passed successfully!" << std::endl;
    return 0;
}
