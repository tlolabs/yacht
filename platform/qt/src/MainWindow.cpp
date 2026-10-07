#include "MainWindow.h"
#include "AppIdentity.h"
#include "DesktopServices.h"
#include "Presets.h"
#include "UpdateClient.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStyleFactory>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace Yacht {

MainWindow::MainWindow(std::shared_ptr<IDesktopServices> desktop, QWidget *parent)
    : QMainWindow(parent), m_desktop(std::move(desktop)) {
    if (!m_desktop) {
        m_desktop = std::make_shared<QtDesktopServices>(this);
    }
    setupUi();
    setupActions();

    m_previewDebounceTimer.setSingleShot(true);
    m_previewDebounceTimer.setInterval(180);
    connect(&m_previewDebounceTimer, &QTimer::timeout, this, &MainWindow::onPerformRender);

    m_updateTimer.setInterval(3600000); // 1 hour
    connect(&m_updateTimer, &QTimer::timeout, this, [this] { checkUpdates(true); });
}

MainWindow::~MainWindow() {
    m_updateTimer.stop();
    m_previewDebounceTimer.stop();
    m_preferences.save();
    if (m_table) {
        m_table->release();
    }
}

void MainWindow::setupUi() {
    setWindowTitle(AppIdentity::title());
    resize(1180, 800);
    setMinimumSize(840, 560);
    setAcceptDrops(true);
    setWindowIcon(QIcon(QStringLiteral(":/icons/yacht.png")));

    // Top Action Buttons Bar
    auto *topBarWidget = new QWidget(this);
    auto *topBarLayout = new QHBoxLayout(topBarWidget);
    topBarLayout->setContentsMargins(12, 6, 12, 6);
    topBarLayout->setSpacing(8);

    auto *openBtn = new QPushButton(QStringLiteral("Open…"), this);
    auto *batchBtn = new QPushButton(QStringLiteral("Batch…"), this);
    auto *sampleBtn = new QPushButton(QStringLiteral("Sample"), this);
    auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"), this);
    auto *copyBtn = new QPushButton(QStringLiteral("Copy HTML"), this);
    auto *exportBtn = new QPushButton(QStringLiteral("Export…"), this);
    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancelButton->setEnabled(false);
    auto *settingsBtn = new QPushButton(QStringLiteral("Settings…"), this);

    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpen);
    connect(batchBtn, &QPushButton::clicked, this, &MainWindow::onBatch);
    connect(sampleBtn, &QPushButton::clicked, this, &MainWindow::onSample);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefresh);
    connect(copyBtn, &QPushButton::clicked, this, &MainWindow::onCopy);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::onCancel);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettings);

    topBarLayout->addWidget(openBtn);
    topBarLayout->addWidget(batchBtn);
    topBarLayout->addWidget(sampleBtn);
    topBarLayout->addWidget(refreshBtn);
    topBarLayout->addWidget(copyBtn);
    topBarLayout->addWidget(exportBtn);
    topBarLayout->addWidget(m_cancelButton);
    topBarLayout->addWidget(settingsBtn);
    topBarLayout->addStretch();

    // Splitter
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // Left Sidebar: Presets & Style Fields
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setMinimumWidth(310);
    scrollArea->setMaximumWidth(400);

    m_sidebarWidget = new QWidget(scrollArea);
    auto *sidebarLayout = new QVBoxLayout(m_sidebarWidget);
    sidebarLayout->setContentsMargins(12, 12, 12, 12);
    sidebarLayout->setSpacing(8);

    auto *presetLabel = new QLabel(QStringLiteral("Preset"), m_sidebarWidget);
    sidebarLayout->addWidget(presetLabel);

    m_presetsCombo = new QComboBox(m_sidebarWidget);
    m_presetsCombo->setAccessibleName(QStringLiteral("Preset"));
    sidebarLayout->addWidget(m_presetsCombo);

    auto *presetButtonsLayout = new QHBoxLayout();
    auto *loadPresetBtn = new QPushButton(QStringLiteral("Load"), m_sidebarWidget);
    auto *deletePresetBtn = new QPushButton(QStringLiteral("Delete"), m_sidebarWidget);
    connect(loadPresetBtn, &QPushButton::clicked, this, &MainWindow::onLoadPreset);
    connect(deletePresetBtn, &QPushButton::clicked, this, &MainWindow::onDeletePreset);
    presetButtonsLayout->addWidget(loadPresetBtn);
    presetButtonsLayout->addWidget(deletePresetBtn);
    sidebarLayout->addLayout(presetButtonsLayout);

    m_savePresetEdit = new QLineEdit(m_sidebarWidget);
    m_savePresetEdit->setPlaceholderText(QStringLiteral("Save preset as"));
    m_savePresetEdit->setAccessibleName(QStringLiteral("Save preset as"));
    sidebarLayout->addWidget(m_savePresetEdit);

    auto *savePresetBtn = new QPushButton(QStringLiteral("Save Current"), m_sidebarWidget);
    connect(savePresetBtn, &QPushButton::clicked, this, &MainWindow::onSavePreset);
    sidebarLayout->addWidget(savePresetBtn);

    auto *resetButtonsLayout = new QHBoxLayout();
    auto *styledBtn = new QPushButton(QStringLiteral("Reset Styled"), m_sidebarWidget);
    auto *unstyledBtn = new QPushButton(QStringLiteral("Reset Unstyled"), m_sidebarWidget);
    connect(styledBtn, &QPushButton::clicked, this, &MainWindow::onResetStyled);
    connect(unstyledBtn, &QPushButton::clicked, this, &MainWindow::onResetUnstyled);
    resetButtonsLayout->addWidget(styledBtn);
    resetButtonsLayout->addWidget(unstyledBtn);
    sidebarLayout->addLayout(resetButtonsLayout);

    // Style Fields
    static const QStringList labels = {
        QStringLiteral("Table class"),       QStringLiteral("Font family"),
        QStringLiteral("Font size"),         QStringLiteral("Cell padding"),
        QStringLiteral("Border width"),      QStringLiteral("Border style"),
        QStringLiteral("Border color"),      QStringLiteral("Header background"),
        QStringLiteral("Header text color"), QStringLiteral("Body background"),
        QStringLiteral("Zebra striping"),    QStringLiteral("Zebra color"),
        QStringLiteral("Hover highlight"),   QStringLiteral("Hover color"),
        QStringLiteral("Border collapse"),   QStringLiteral("Border spacing")};

    static const QStringList keys = {
        QStringLiteral("table_class"),       QStringLiteral("font_family"),
        QStringLiteral("font_size_px"),      QStringLiteral("cell_padding_px"),
        QStringLiteral("border_width_px"),   QStringLiteral("border_style"),
        QStringLiteral("border_color"),      QStringLiteral("header_bg"),
        QStringLiteral("header_text_color"), QStringLiteral("body_bg"),
        QStringLiteral("zebra_enabled"),     QStringLiteral("zebra_bg"),
        QStringLiteral("hover_enabled"),     QStringLiteral("hover_bg"),
        QStringLiteral("border_collapse"),   QStringLiteral("border_spacing_px")};

    for (int i = 0; i < keys.size(); ++i) {
        QJsonValue defVal;
        if (keys[i].endsWith(QStringLiteral("_enabled"))) {
            defVal = false;
        } else if (keys[i].endsWith(QStringLiteral("_px"))) {
            defVal = 0;
        } else {
            defVal = QString();
        }
        auto *field = new StyleField(keys[i], labels[i], defVal, m_sidebarWidget);
        connect(field, &StyleField::valueChanged, this, &MainWindow::onStyleFieldChanged);
        sidebarLayout->addWidget(field);
        m_styleFields.push_back(field);
    }
    sidebarLayout->addStretch();
    scrollArea->setWidget(m_sidebarWidget);
    splitter->addWidget(scrollArea);

    // Right Panel: Summary, Controls, Tabs, Note
    auto *rightWidget = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(12, 0, 12, 0);
    rightLayout->setSpacing(8);

    m_summaryLabel = new QLabel(QStringLiteral("Built-in sample"), rightWidget);
    m_summaryLabel->setWordWrap(true);
    rightLayout->addWidget(m_summaryLabel);

    auto *controlsRowLayout = new QHBoxLayout();
    controlsRowLayout->setSpacing(12);

    auto *delimLayout = new QVBoxLayout();
    auto *delimLabel = new QLabel(QStringLiteral("Delimiter"), rightWidget);
    m_delimiterCombo = new QComboBox(rightWidget);
    m_delimiterCombo->setAccessibleName(QStringLiteral("Delimiter"));
    m_delimiterCombo->addItems({QStringLiteral("comma"), QStringLiteral("tab"),
                                QStringLiteral("semicolon"), QStringLiteral("pipe")});
    connect(m_delimiterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onDelimiterChanged);
    delimLayout->addWidget(delimLabel);
    delimLayout->addWidget(m_delimiterCombo);
    controlsRowLayout->addLayout(delimLayout);

    auto *recentLayout = new QVBoxLayout();
    auto *recentLabel = new QLabel(QStringLiteral("Recent files"), rightWidget);
    m_recentCombo = new QComboBox(rightWidget);
    m_recentCombo->setAccessibleName(QStringLiteral("Recent files"));
    m_recentCombo->setMinimumWidth(260);
    recentLayout->addWidget(recentLabel);
    recentLayout->addWidget(m_recentCombo);
    controlsRowLayout->addLayout(recentLayout);

    auto *openRecentBtn = new QPushButton(QStringLiteral("Open Recent"), rightWidget);
    connect(openRecentBtn, &QPushButton::clicked, this, &MainWindow::onOpenRecent);
    controlsRowLayout->addWidget(openRecentBtn, 0, Qt::AlignBottom);
    controlsRowLayout->addStretch();
    rightLayout->addLayout(controlsRowLayout);

    // Tab Widget
    m_tabWidget = new QTabWidget(rightWidget);
    m_previewWidget = new HtmlPreviewWidget(m_tabWidget);
    m_sourceEdit = new QPlainTextEdit(m_tabWidget);
    m_sourceEdit->setReadOnly(true);
    m_sourceEdit->setAccessibleName(QStringLiteral("HTML Source"));
    QFont monoFont(QStringLiteral("monospace"));
    monoFont.setStyleHint(QFont::Monospace);
    m_sourceEdit->setFont(monoFont);

    m_tabWidget->addTab(m_previewWidget, QStringLiteral("Table Preview"));
    m_tabWidget->addTab(m_sourceEdit, QStringLiteral("HTML Source"));
    rightLayout->addWidget(m_tabWidget, 1);

    m_noteLabel = new QLabel(rightWidget);
    m_noteLabel->setWordWrap(true);
    rightLayout->addWidget(m_noteLabel);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    // Bottom Status Bar
    auto *bottomWidget = new QWidget(this);
    auto *bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(12, 6, 12, 12);
    bottomLayout->setSpacing(6);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // indeterminate
    m_progressBar->setFixedHeight(4);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    m_progressBar->setAccessibleName(QStringLiteral("Conversion in progress"));
    bottomLayout->addWidget(m_progressBar);

    auto *statusRowLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(QStringLiteral("Ready"), this);
    m_statusLabel->setWordWrap(true);
    statusRowLayout->addWidget(m_statusLabel, 1);

    m_revealButton = new QPushButton(QStringLiteral("Reveal file"), this);
    m_browserButton = new QPushButton(QStringLiteral("Open in Browser"), this);
    connect(m_revealButton, &QPushButton::clicked, this, &MainWindow::onRevealLastExport);
    connect(m_browserButton, &QPushButton::clicked, this, &MainWindow::onOpenLastExportInBrowser);
    statusRowLayout->addWidget(m_revealButton);
    statusRowLayout->addWidget(m_browserButton);
    bottomLayout->addLayout(statusRowLayout);

    // Main Central Layout
    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(topBarWidget);
    centralLayout->addWidget(splitter, 1);
    centralLayout->addWidget(bottomWidget);
    setCentralWidget(central);
}

void MainWindow::setupActions() {
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    m_openAction = fileMenu->addAction(QStringLiteral("&Open…"), QKeySequence::Open, this,
                                      &MainWindow::onOpen);
    m_batchAction = fileMenu->addAction(QStringLiteral("&Batch Convert…"),
                                        QKeySequence(QStringLiteral("Ctrl+Shift+B")), this,
                                        &MainWindow::onBatch);
    m_exportAction = fileMenu->addAction(QStringLiteral("&Export HTML…"), QKeySequence::Save, this,
                                         &MainWindow::onExport);
    m_copyAction = fileMenu->addAction(QStringLiteral("&Copy HTML"),
                                       QKeySequence(QStringLiteral("Ctrl+Shift+C")), this,
                                       &MainWindow::onCopy);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    m_previewTabAction = viewMenu->addAction(QStringLiteral("Table &Preview"),
                                             QKeySequence(QStringLiteral("Ctrl+1")), this,
                                             [this] { switchTab(0); });
    m_sourceTabAction = viewMenu->addAction(QStringLiteral("HTML &Source"),
                                            QKeySequence(QStringLiteral("Ctrl+2")), this,
                                            [this] { switchTab(1); });
    m_refreshAction = viewMenu->addAction(QStringLiteral("&Refresh"), QKeySequence::Refresh, this,
                                          &MainWindow::onRefresh);
    viewMenu->addSeparator();
    m_settingsAction = viewMenu->addAction(QStringLiteral("&Settings…"), this, &MainWindow::onSettings);

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("Check for Updates…"), this, [this] { checkUpdates(false); });
    helpMenu->addAction(QStringLiteral("Enable Automatic Update Checks"), this, [this] {
        try {
            if (AppIdentity::isInternal()) {
                m_desktop->showMessage(QStringLiteral("Internal reference build"),
                                       QStringLiteral("Production updates are disabled for this internal application."));
            } else {
                UpdateClient::run(QStringLiteral("enable"));
            }
        } catch (const std::exception &e) {
            m_desktop->showMessage(QStringLiteral("YACHT"), QString::fromUtf8(e.what()));
        }
    });
    helpMenu->addAction(QStringLiteral("Disable Automatic Update Checks"), this, [this] {
        try {
            if (AppIdentity::isInternal()) {
                m_desktop->showMessage(QStringLiteral("Internal reference build"),
                                       QStringLiteral("Production updates are disabled for this internal application."));
            } else {
                UpdateClient::run(QStringLiteral("disable"));
            }
        } catch (const std::exception &e) {
            m_desktop->showMessage(QStringLiteral("YACHT"), QString::fromUtf8(e.what()));
        }
    });
    helpMenu->addSeparator();
    helpMenu->addAction(QStringLiteral("Download Releases"), this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/tlolabs/yacht/releases")));
    });
    helpMenu->addAction(QStringLiteral("User Guide"), this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/tlolabs/yacht#using-yacht")));
    });
}

void MainWindow::initialize() {
    QString warning;
    m_preferences = Preferences::load();
    if (!m_preferences.readable) {
        warning = QStringLiteral("Could not load preferences; the original file will be preserved.");
    }
    applyAppearance(m_preferences.appearance);
    syncRecent();

    QJsonObject initStyle = YachtCore::style(QStringLiteral("defaults"));
    try {
        if (m_preferences.rememberStyle && !m_preferences.lastStyle.isEmpty()) {
            QJsonObject normArgs;
            normArgs[QStringLiteral("style")] = m_preferences.lastStyle;
            initStyle = YachtCore::call(QStringLiteral("normalize_style"), normArgs).toObject();
            QJsonObject valArgs;
            valArgs[QStringLiteral("style")] = initStyle;
            YachtCore::call(QStringLiteral("validate_style"), valArgs);
        }
        m_presets = Presets::load();
    } catch (const std::exception &e) {
        warning = QStringLiteral("Could not load saved style/presets; the original files will be preserved: ") +
                  QString::fromUtf8(e.what());
        initStyle = YachtCore::style(QStringLiteral("defaults"));
    }

    syncPresets();
    m_table = YachtCore::sample();
    setStyle(initStyle);
    onPerformRender();

    if (!warning.isEmpty()) {
        m_statusLabel->setText(warning);
        m_desktop->showMessage(QStringLiteral("Settings could not be loaded"), warning);
    } else {
        m_statusLabel->setText(QStringLiteral("Ready"));
    }

    m_updateTimer.start();
    checkUpdates(true);
}

void MainWindow::setBusy(bool busy) {
    m_busy = busy;
    m_progressBar->setVisible(busy);
    m_cancelButton->setEnabled(busy);
    m_sidebarWidget->setEnabled(!busy);
    m_delimiterCombo->setEnabled(!busy);
    m_recentCombo->setEnabled(!busy);
    m_openAction->setEnabled(!busy);
    m_batchAction->setEnabled(!busy);
    m_exportAction->setEnabled(!busy);
    m_copyAction->setEnabled(!busy);
    m_refreshAction->setEnabled(!busy);
}

void MainWindow::applyAppearance(const QString &appearance) {
    if (appearance == QStringLiteral("Dark")) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
        darkPalette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        QApplication::setPalette(darkPalette);
    } else if (appearance == QStringLiteral("Light")) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(QApplication::style()->standardPalette());
    } else {
        // System
        QApplication::setPalette(QApplication::style()->standardPalette());
    }
}

void MainWindow::syncPresets() {
    m_presetsCombo->clear();
    m_presetsCombo->addItem(QStringLiteral("Default (Styled)"));
    m_presetsCombo->addItem(QStringLiteral("Unstyled"));
    QStringList custom = m_presets.keys();
    custom.sort(Qt::CaseInsensitive);
    for (const QString &p : custom) {
        m_presetsCombo->addItem(p);
    }
    m_presetsCombo->setCurrentIndex(0);
}

void MainWindow::syncRecent() {
    m_recentCombo->clear();
    for (const QString &path : m_preferences.recent) {
        m_recentCombo->addItem(path);
    }
}

void MainWindow::savePreferences() {
    if (!m_preferences.readable) {
        m_statusLabel->setText(
            QStringLiteral("Preferences could not be read. Repair the file and restart before saving settings."));
        return;
    }
    if (!m_preferences.save()) {
        m_statusLabel->setText(QStringLiteral("Could not save preferences."));
    }
}

void MainWindow::setStyle(const QJsonObject &style) {
    for (StyleField *field : m_styleFields) {
        if (style.contains(field->key())) {
            field->setValue(style.value(field->key()));
        }
    }
    m_previewDebounceTimer.start();
}

QJsonObject MainWindow::currentStyle() const {
    QJsonObject obj;
    for (const StyleField *field : m_styleFields) {
        obj[field->key()] = field->value();
    }
    QJsonObject args;
    args[QStringLiteral("style")] = obj;
    YachtCore::call(QStringLiteral("validate_style"), args);
    return obj;
}

StyleField *MainWindow::findField(const QString &key) const {
    for (StyleField *f : m_styleFields) {
        if (f->key() == key) return f;
    }
    return nullptr;
}

QString MainWindow::statusText() const {
    return m_statusLabel->text();
}

QString MainWindow::summaryText() const {
    return m_summaryLabel->text();
}

QString MainWindow::noteText() const {
    return m_noteLabel->text();
}

QString MainWindow::delimiter() const {
    return m_delimiter;
}

void MainWindow::setDelimiter(const QString &delim) {
    m_delimiter = delim;
    int idx = m_delimiterCombo->findText(delim);
    if (idx >= 0) {
        m_delimiterCombo->setCurrentIndex(idx);
    }
}

QString MainWindow::currentPresetName() const {
    return m_presetsCombo->currentText();
}

void MainWindow::setPresetNameInput(const QString &name) {
    m_savePresetEdit->setText(name);
}

QStringList MainWindow::presetNames() const {
    QStringList names;
    for (int i = 0; i < m_presetsCombo->count(); ++i) {
        names.append(m_presetsCombo->itemText(i));
    }
    return names;
}

void MainWindow::selectPreset(const QString &name) {
    int idx = m_presetsCombo->findText(name);
    if (idx >= 0) {
        m_presetsCombo->setCurrentIndex(idx);
    }
}

void MainWindow::switchTab(int index) {
    m_tabWidget->setCurrentIndex(index);
}

void MainWindow::onStyleFieldChanged() {
    m_previewDebounceTimer.start();
}

void MainWindow::onPerformRender() {
    if (!m_table) return;

    try {
        QJsonObject styleObj = currentStyle();
        int limit = m_preferences.previewRows;

        QJsonObject args;
        args[QStringLiteral("style")] = styleObj;
        args[QStringLiteral("limit")] = limit;

        QJsonObject result = m_table->call(QStringLiteral("preview"), args,
                                           [this] { return m_cancelled.load(); }).toObject();

        QString html = result.value(QStringLiteral("html")).toString();
        QString source = result.value(QStringLiteral("source")).toString();
        int rowCount = result.value(QStringLiteral("row_count")).toInt();
        int totalRows = m_table->metadata().value(QStringLiteral("row_count")).toInt();
        bool unavailable = result.value(QStringLiteral("preview_unavailable")).toBool();
        bool truncated = result.value(QStringLiteral("source_truncated")).toBool();

        m_previewWidget->setHtml(html);
        m_sourceEdit->setPlainText(source);

        QString note = unavailable ? QStringLiteral("Cells too large to preview. ")
                                   : QStringLiteral("Preview shows %1 of %2 rows. ").arg(rowCount).arg(totalRows);
        if (truncated) {
            note += QStringLiteral("Source shows the first 1 MB. ");
        }
        note += QStringLiteral("Copy and Export include every row.");
        m_noteLabel->setText(note);

        m_preferences.lastStyle = m_preferences.rememberStyle ? styleObj : QJsonObject();
        savePreferences();
    } catch (const YachtCancelledException &) {
        m_statusLabel->setText(QStringLiteral("Cancelled"));
    } catch (const std::exception &e) {
        m_statusLabel->setText(QString::fromUtf8(e.what()));
        m_previewWidget->clear();
        m_sourceEdit->clear();
    }
}

void MainWindow::loadFile(const QString &path, bool infer) {
    if (m_busy) return;
    setBusy(true);
    if (m_cancelled.load()) {
        m_cancelled = false;
        setBusy(false);
        m_statusLabel->setText(QStringLiteral("Cancelled"));
        return;
    }
    m_cancelled = false;

    m_previewWidget->clear();
    m_sourceEdit->clear();

    if (infer && path.endsWith(QStringLiteral(".tsv"), Qt::CaseInsensitive)) {
        setDelimiter(QStringLiteral("tab"));
    }

    m_statusLabel->setText(QStringLiteral("Reading ") + path);
    QApplication::processEvents();

    try {
        TablePtr nextTable = YachtCore::read(path, m_delimiter, [this] { return m_cancelled.load(); });
        if (m_cancelled.load()) {
            nextTable->release();
            throw YachtCancelledException();
        }

        if (m_table) {
            m_table->release();
        }
        m_table = nextTable;
        m_sourcePath = path;

        QJsonObject meta = m_table->metadata();
        int rows = meta.value(QStringLiteral("row_count")).toInt();
        int cols = meta.value(QStringLiteral("header")).toArray().size();
        QStringList warnings;
        for (const auto &w : meta.value(QStringLiteral("warnings")).toArray()) {
            warnings.append(w.toString());
        }

        QString summary = QStringLiteral("%1 · %2 rows · %3 columns\n%4")
                              .arg(QFileInfo(path).fileName())
                              .arg(rows)
                              .arg(cols)
                              .arg(warnings.join(QLatin1Char(' ')));
        m_summaryLabel->setText(summary.trimmed());

        m_preferences.recent.removeAll(path);
        m_preferences.recent.prepend(path);
        while (m_preferences.recent.size() > 10) m_preferences.recent.removeLast();
        syncRecent();
        savePreferences();

        m_statusLabel->setText(QStringLiteral("Loaded ") + path);
        setBusy(false);
        onPerformRender();
    } catch (const YachtCancelledException &) {
        setBusy(false);
        m_statusLabel->setText(QStringLiteral("Cancelled"));
    } catch (const std::exception &e) {
        setBusy(false);
        m_statusLabel->setText(QString::fromUtf8(e.what()));
        m_desktop->showMessage(QStringLiteral("YACHT"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::onOpen() {
    if (m_busy) return;
    receive(m_desktop->openFiles(), false);
}

void MainWindow::onBatch() {
    if (m_busy) return;
    receive(m_desktop->openFiles(), true);
}

void MainWindow::onSample() {
    if (m_busy) return;
    m_sourcePath.clear();
    if (m_table) m_table->release();
    m_table = YachtCore::sample();
    m_summaryLabel->setText(QStringLiteral("Built-in sample · 9 rows · 3 columns"));
    onPerformRender();
}

void MainWindow::onRefresh() {
    if (m_sourcePath.isEmpty()) {
        onPerformRender();
    } else {
        loadFile(m_sourcePath, false);
    }
}

void MainWindow::onCopy() {
    copyHtmlToClipboard();
}

void MainWindow::copyHtmlToClipboard() {
    if (m_busy || !m_table) return;
    setBusy(true);
    m_cancelled = false;
    try {
        QJsonObject args;
        args[QStringLiteral("style")] = currentStyle();
        QJsonValue htmlVal = m_table->call(QStringLiteral("html"), args, [this] { return m_cancelled.load(); });
        QString html = htmlVal.toString();
        m_desktop->setClipboard(html);
        m_statusLabel->setText(QStringLiteral("Copied complete HTML document"));
    } catch (const YachtCancelledException &) {
        m_statusLabel->setText(QStringLiteral("Cancelled"));
    } catch (const std::exception &e) {
        m_statusLabel->setText(QString::fromUtf8(e.what()));
        m_desktop->showMessage(QStringLiteral("YACHT"), QString::fromUtf8(e.what()));
    }
    setBusy(false);
}

void MainWindow::onExport() {
    if (m_busy) return;
    QString defaultName = m_sourcePath.isEmpty()
                              ? QStringLiteral("YACHT Table.html")
                              : QFileInfo(m_sourcePath).completeBaseName() + QStringLiteral(".html");
    QString path = m_desktop->saveFile(defaultName);
    if (!path.isEmpty()) {
        exportTo(path, true);
    }
}

void MainWindow::exportTo(const QString &path, bool overwrite) {
    if (m_busy || !m_table) return;
    setBusy(true);
    m_cancelled = false;
    try {
        QJsonObject args;
        args[QStringLiteral("style")] = currentStyle();
        args[QStringLiteral("path")] = path;
        args[QStringLiteral("overwrite")] = overwrite;
        m_table->call(QStringLiteral("export"), args, [this] { return m_cancelled.load(); });
        m_lastExport = path;
        m_statusLabel->setText(QStringLiteral("Exported ") + QFileInfo(path).fileName());
    } catch (const YachtCancelledException &) {
        m_statusLabel->setText(QStringLiteral("Cancelled"));
    } catch (const std::exception &e) {
        m_statusLabel->setText(QString::fromUtf8(e.what()));
        setBusy(false);
        throw;
    }
    setBusy(false);
}

void MainWindow::onCancel() {
    cancelActiveWork();
}

void MainWindow::cancelActiveWork() {
    m_cancelled = true;
    m_previewDebounceTimer.stop();
    m_statusLabel->setText(QStringLiteral("Cancellation requested"));
}

void MainWindow::onSettings() {
    if (m_busy) return;
    setBusy(true);
    if (m_desktop->settings(m_preferences)) {
        syncRecent();
        applyAppearance(m_preferences.appearance);
        savePreferences();
        onPerformRender();
    }
    setBusy(false);
}

void MainWindow::onResetStyled() {
    setStyle(YachtCore::style(QStringLiteral("defaults")));
}

void MainWindow::onResetUnstyled() {
    setStyle(YachtCore::style(QStringLiteral("unstyled")));
}

void MainWindow::onLoadPreset() {
    loadSelectedPreset();
}

void MainWindow::loadSelectedPreset() {
    QString sel = m_presetsCombo->currentText();
    if (sel == QStringLiteral("Unstyled")) {
        setStyle(YachtCore::style(QStringLiteral("unstyled")));
    } else if (sel == QStringLiteral("Default (Styled)")) {
        setStyle(YachtCore::style(QStringLiteral("defaults")));
    } else if (m_presets.contains(sel)) {
        setStyle(m_presets.value(sel).toObject());
    } else {
        setStyle(YachtCore::style(QStringLiteral("defaults")));
    }
}

void MainWindow::onSavePreset() {
    saveCurrentPreset();
}

void MainWindow::saveCurrentPreset() {
    if (!m_preferences.readable) {
        m_desktop->showMessage(QStringLiteral("YACHT"),
                               QStringLiteral("Repair the unreadable preset store and restart before saving presets."));
        return;
    }
    QString name = m_savePresetEdit->text().trimmed();
    if (name.isEmpty() || name == QStringLiteral("Unstyled") || name == QStringLiteral("Default (Styled)")) {
        m_desktop->showMessage(QStringLiteral("YACHT"),
                               QStringLiteral("Choose a preset name other than a built-in preset name."));
        return;
    }

    if (m_presets.contains(name)) {
        if (!m_desktop->confirm(QStringLiteral("Replace preset?"), name, QStringLiteral("Replace"))) {
            return;
        }
    }

    try {
        QJsonObject nextPresets = m_presets;
        nextPresets[name] = currentStyle();
        Presets::save(nextPresets);
        m_presets = nextPresets;
        syncPresets();
        selectPreset(name);
        m_statusLabel->setText(QStringLiteral("Saved preset ") + name);
    } catch (const std::exception &e) {
        m_statusLabel->setText(QString::fromUtf8(e.what()));
        m_desktop->showMessage(QStringLiteral("YACHT"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::onDeletePreset() {
    deleteSelectedPreset();
}

void MainWindow::deleteSelectedPreset() {
    QString name = m_presetsCombo->currentText();
    if (name.isEmpty() || name == QStringLiteral("Unstyled") || name == QStringLiteral("Default (Styled)")) {
        return;
    }
    if (!m_presets.contains(name)) {
        return;
    }

    if (!m_desktop->confirm(QStringLiteral("Delete preset?"), name, QStringLiteral("Delete"))) {
        return;
    }

    try {
        QJsonObject nextPresets = m_presets;
        nextPresets.remove(name);
        Presets::save(nextPresets);
        m_presets = nextPresets;
        syncPresets();
        m_statusLabel->setText(QStringLiteral("Deleted preset ") + name);
    } catch (const std::exception &e) {
        m_statusLabel->setText(QString::fromUtf8(e.what()));
        m_desktop->showMessage(QStringLiteral("YACHT"), QString::fromUtf8(e.what()));
    }
}

void MainWindow::onOpenRecent() {
    QString path = m_recentCombo->currentText();
    if (!path.isEmpty()) {
        loadFile(path);
    }
}

void MainWindow::onRevealLastExport() {
    if (m_lastExport.isEmpty()) return;
    m_desktop->reveal(m_lastExport);
}

void MainWindow::onOpenLastExportInBrowser() {
    if (m_lastExport.isEmpty()) return;
    m_desktop->open(m_lastExport);
}

void MainWindow::onDelimiterChanged(int index) {
    Q_UNUSED(index);
    QString newDelim = m_delimiterCombo->currentText();
    if (newDelim != m_delimiter) {
        m_delimiter = newDelim;
        if (!m_busy) {
            onRefresh();
        }
    }
}

void MainWindow::receive(const QStringList &paths, bool batch) {
    if (paths.isEmpty() || m_busy) return;
    if (paths.size() > 1 || batch) {
        batchConvert(paths);
    } else {
        loadFile(paths.first());
    }
}

void MainWindow::batchConvert(const QStringList &paths) {
    if (m_busy || paths.isEmpty()) return;
    setBusy(true);

    auto overwriteOpt = m_desktop->reviewBatch(paths);
    if (!overwriteOpt.has_value()) {
        setBusy(false);
        return;
    }

    bool overwrite = *overwriteOpt;
    if (overwrite) {
        if (!m_desktop->confirm(
                QStringLiteral("Replace existing HTML?"),
                QStringLiteral("CSV inputs are preserved. Existing matching HTML will be replaced."),
                QStringLiteral("Replace and Convert"))) {
            setBusy(false);
            return;
        }
    }

    QStringList results;
    m_cancelled = false;
    QJsonObject styleObj = currentStyle();
    QString separator = m_delimiter;

    for (int i = 0; i < paths.size(); ++i) {
        if (m_cancelled.load()) {
            results.append(QStringLiteral("Cancelled; completed files are preserved."));
            break;
        }

        const QString &p = paths[i];
        try {
            QJsonObject args;
            QJsonArray inArr;
            inArr.append(p);
            args[QStringLiteral("inputs")] = inArr;
            args[QStringLiteral("style")] = styleObj;
            args[QStringLiteral("delimiter")] = separator;
            args[QStringLiteral("overwrite")] = overwrite;

            QJsonArray batchRes = YachtCore::call(QStringLiteral("batch"), args,
                                                  [this] { return m_cancelled.load(); }).toArray();
            QJsonObject r = batchRes.first().toObject();
            if (r.contains(QStringLiteral("error")) && !r.value(QStringLiteral("error")).isNull()) {
                results.append(p + QStringLiteral(": ") + r.value(QStringLiteral("error")).toString());
            } else {
                QString out = r.value(QStringLiteral("output")).toString();
                results.append(QStringLiteral("Saved ") + out);
                m_lastExport = out;
            }
            m_statusLabel->setText(
                QStringLiteral("Batch processed %1 of %2 files").arg(results.size()).arg(paths.size()));
            QApplication::processEvents();
        } catch (const YachtCancelledException &) {
            results.append(QStringLiteral("Cancelled; completed files are preserved."));
            break;
        } catch (const std::exception &e) {
            results.append(p + QStringLiteral(": ") + QString::fromUtf8(e.what()));
        }
    }

    setBusy(false);
    m_desktop->showMessage(QStringLiteral("Batch results"), results.join(QLatin1Char('\n')));
}

void MainWindow::checkUpdates(bool automatic) {
    if (m_busy) return;
    if (AppIdentity::isInternal()) {
        if (!automatic) {
            m_desktop->showMessage(QStringLiteral("Internal reference build"),
                                   QStringLiteral("Production updates are disabled. Obtain reference builds from CI artifacts."));
        }
        return;
    }

    try {
        QJsonObject update = UpdateClient::run(automatic ? QStringLiteral("check-auto")
                                                         : QStringLiteral("check"));
        QString status = update.value(QStringLiteral("status")).toString();
        if (status != QStringLiteral("available")) {
            if (!automatic) {
                m_desktop->showMessage(QStringLiteral("Updates"),
                                       QStringLiteral("No compatible newer update was found. Production updates may not yet be configured; published packages are available from Help → Download Releases."));
            }
            return;
        }

        QString ver = update.value(QStringLiteral("version")).toString();
        if (automatic) {
            if (!m_busy) {
                m_statusLabel->setText(QStringLiteral("YACHT %1 is available. Use Help → Check for Updates…").arg(ver));
            }
            return;
        }

#if !defined(Q_OS_WIN)
        m_desktop->showMessage(
            QStringLiteral("Update available"),
            QStringLiteral("YACHT %1 is available. Install the published package using your distribution's package manager. Export your work first.").arg(ver));
#else
        if (!m_desktop->confirm(
            QStringLiteral("Update available"),
            QStringLiteral("Download YACHT %1? Export your work before closing YACHT to install.").arg(ver),
            QStringLiteral("Download"))) {
            return;
        }

        QJsonObject download = UpdateClient::run(QStringLiteral("download"),
                                                 {ver, update.value(QStringLiteral("sha256")).toString()});
        if (download.value(QStringLiteral("migration")).toString() != QStringLiteral("none")) {
            throw std::runtime_error("This release requires manual migration; see the release notes.");
        }

        UpdateClient::verifyInstaller(download);
        m_desktop->showMessage(
            QStringLiteral("Verified installer ready"),
            QStringLiteral("Signature, publisher and checksum verified. Export your work, close YACHT, then run the installer from the folder that opens next."));
        m_lastExport = download.value(QStringLiteral("path")).toString();
        onRevealLastExport();
#endif
    } catch (const std::exception &e) {
        if (!automatic) {
            m_desktop->showMessage(QStringLiteral("Updates"), QString::fromUtf8(e.what()));
        }
    }
}

bool MainWindow::mayClose() {
    if (!m_busy) return true;
    return m_desktop->confirm(
        QStringLiteral("Work is in progress"),
        QStringLiteral("Cancel the active operation and close YACHT? Completed exports will be preserved."),
        QStringLiteral("Cancel Work and Close"));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_closeConfirmed) {
        event->accept();
        return;
    }

    if (!mayClose()) {
        event->ignore();
        return;
    }

    m_closeConfirmed = true;
    cancelActiveWork();
    m_updateTimer.stop();
    m_previewDebounceTimer.stop();
    savePreferences();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (!m_busy && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    if (m_busy) return;
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths.append(url.toLocalFile());
        }
    }
    receive(paths);
}

} // namespace Yacht
