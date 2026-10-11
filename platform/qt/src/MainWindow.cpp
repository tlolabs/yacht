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
#include <QEventLoop>
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
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <chrono>
#include <future>

namespace Yacht {

namespace {

// Keep the event loop servicing Cancel while the Rust core works on a worker thread.
// The result is still delivered synchronously to existing callers on the GUI thread.
template <typename Work>
auto runCoreResponsive(Work work) -> decltype(work()) {
    auto result = std::async(std::launch::async, std::move(work));
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(16);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (result.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            loop.quit();
        }
    });
    poll.start();
    loop.exec();
    return result.get();
}

} // namespace

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
    openBtn->setAccessibleName(QStringLiteral("Open CSV…"));
    openBtn->setAccessibleDescription(QStringLiteral("Open a CSV or TSV file (Ctrl+O)"));
    openBtn->setToolTip(QStringLiteral("Open a CSV or TSV file (Ctrl+O)"));

    auto *batchBtn = new QPushButton(QStringLiteral("Batch…"), this);
    batchBtn->setAccessibleName(QStringLiteral("Batch Convert…"));
    batchBtn->setAccessibleDescription(QStringLiteral("Batch convert multiple CSV files to HTML (Ctrl+Shift+B)"));
    batchBtn->setToolTip(QStringLiteral("Batch convert multiple CSV files to HTML (Ctrl+Shift+B)"));

    auto *sampleBtn = new QPushButton(QStringLiteral("Sample"), this);
    sampleBtn->setAccessibleName(QStringLiteral("Load Sample Table"));
    sampleBtn->setAccessibleDescription(QStringLiteral("Load the built-in sample table"));
    sampleBtn->setToolTip(QStringLiteral("Load the built-in sample table"));

    auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"), this);
    refreshBtn->setAccessibleName(QStringLiteral("Refresh Preview"));
    refreshBtn->setAccessibleDescription(QStringLiteral("Refresh preview with current delimiter (Ctrl+R or F5)"));
    refreshBtn->setToolTip(QStringLiteral("Refresh preview with current delimiter (Ctrl+R or F5)"));

    auto *copyBtn = new QPushButton(QStringLiteral("Copy HTML"), this);
    copyBtn->setAccessibleName(QStringLiteral("Copy HTML Code"));
    copyBtn->setAccessibleDescription(QStringLiteral("Copy complete HTML document to clipboard (Ctrl+Shift+C)"));
    copyBtn->setToolTip(QStringLiteral("Copy complete HTML document to clipboard (Ctrl+Shift+C)"));

    auto *exportBtn = new QPushButton(QStringLiteral("Export…"), this);
    exportBtn->setAccessibleName(QStringLiteral("Export HTML…"));
    exportBtn->setAccessibleDescription(QStringLiteral("Export complete HTML document to file (Ctrl+S)"));
    exportBtn->setToolTip(QStringLiteral("Export complete HTML document to file (Ctrl+S)"));

    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancelButton->setEnabled(false);
    m_cancelButton->setAccessibleName(QStringLiteral("Cancel Operation"));
    m_cancelButton->setAccessibleDescription(QStringLiteral("Cancel current background operation (Escape)"));
    m_cancelButton->setToolTip(QStringLiteral("Cancel current background operation (Escape)"));

    auto *settingsBtn = new QPushButton(QStringLiteral("Settings…"), this);
    settingsBtn->setAccessibleName(QStringLiteral("Settings…"));
    settingsBtn->setAccessibleDescription(QStringLiteral("Open application settings and appearance preferences"));
    settingsBtn->setToolTip(QStringLiteral("Open application settings and appearance preferences (Ctrl+,)"));

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
    m_presetsCombo->setAccessibleDescription(QStringLiteral("Select a style preset"));
    m_presetsCombo->setToolTip(QStringLiteral("Select a table style preset"));
    presetLabel->setBuddy(m_presetsCombo);
    sidebarLayout->addWidget(m_presetsCombo);

    auto *presetButtonsLayout = new QHBoxLayout();
    auto *loadPresetBtn = new QPushButton(QStringLiteral("Load"), m_sidebarWidget);
    loadPresetBtn->setAccessibleName(QStringLiteral("Load Preset"));
    loadPresetBtn->setAccessibleDescription(QStringLiteral("Apply the selected preset"));
    loadPresetBtn->setToolTip(QStringLiteral("Apply the selected preset"));

    auto *deletePresetBtn = new QPushButton(QStringLiteral("Delete"), m_sidebarWidget);
    deletePresetBtn->setAccessibleName(QStringLiteral("Delete Preset"));
    deletePresetBtn->setAccessibleDescription(QStringLiteral("Delete the selected custom preset"));
    deletePresetBtn->setToolTip(QStringLiteral("Delete the selected custom preset"));

    connect(loadPresetBtn, &QPushButton::clicked, this, &MainWindow::onLoadPreset);
    connect(deletePresetBtn, &QPushButton::clicked, this, &MainWindow::onDeletePreset);
    presetButtonsLayout->addWidget(loadPresetBtn);
    presetButtonsLayout->addWidget(deletePresetBtn);
    sidebarLayout->addLayout(presetButtonsLayout);

    m_savePresetEdit = new QLineEdit(m_sidebarWidget);
    m_savePresetEdit->setPlaceholderText(QStringLiteral("Save preset as"));
    m_savePresetEdit->setAccessibleName(QStringLiteral("Save preset as"));
    m_savePresetEdit->setAccessibleDescription(QStringLiteral("Enter a name to save the current style preset"));
    m_savePresetEdit->setToolTip(QStringLiteral("Enter a name to save current style settings as a preset"));
    sidebarLayout->addWidget(m_savePresetEdit);

    auto *savePresetBtn = new QPushButton(QStringLiteral("Save Current"), m_sidebarWidget);
    savePresetBtn->setAccessibleName(QStringLiteral("Save Current Preset"));
    savePresetBtn->setAccessibleDescription(QStringLiteral("Save current style settings under the entered preset name"));
    savePresetBtn->setToolTip(QStringLiteral("Save current style settings under the entered preset name"));
    connect(savePresetBtn, &QPushButton::clicked, this, &MainWindow::onSavePreset);
    sidebarLayout->addWidget(savePresetBtn);

    auto *resetButtonsLayout = new QHBoxLayout();
    auto *styledBtn = new QPushButton(QStringLiteral("Reset Styled"), m_sidebarWidget);
    styledBtn->setAccessibleName(QStringLiteral("Reset Styled"));
    styledBtn->setAccessibleDescription(QStringLiteral("Reset style to built-in default styled settings"));
    styledBtn->setToolTip(QStringLiteral("Reset style to built-in default styled settings"));

    auto *unstyledBtn = new QPushButton(QStringLiteral("Reset Unstyled"), m_sidebarWidget);
    unstyledBtn->setAccessibleName(QStringLiteral("Reset Unstyled"));
    unstyledBtn->setAccessibleDescription(QStringLiteral("Reset style to plain unstyled table"));
    unstyledBtn->setToolTip(QStringLiteral("Reset style to plain unstyled table"));

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
    m_summaryLabel->setAccessibleName(QStringLiteral("Table summary"));
    m_summaryLabel->setAccessibleDescription(QStringLiteral("Information about the current table"));
    rightLayout->addWidget(m_summaryLabel);

    auto *controlsRowLayout = new QHBoxLayout();
    controlsRowLayout->setSpacing(12);

    auto *delimLayout = new QVBoxLayout();
    auto *delimLabel = new QLabel(QStringLiteral("Delimiter"), rightWidget);
    m_delimiterCombo = new QComboBox(rightWidget);
    m_delimiterCombo->setAccessibleName(QStringLiteral("Delimiter"));
    m_delimiterCombo->setAccessibleDescription(QStringLiteral("Choose CSV delimiter character"));
    m_delimiterCombo->setToolTip(QStringLiteral("Delimiter character used to parse fields"));
    delimLabel->setBuddy(m_delimiterCombo);
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
    m_recentCombo->setAccessibleDescription(QStringLiteral("Select a recently opened file"));
    m_recentCombo->setToolTip(QStringLiteral("List of recently opened CSV or TSV files"));
    recentLabel->setBuddy(m_recentCombo);
    m_recentCombo->setMinimumWidth(260);
    recentLayout->addWidget(recentLabel);
    recentLayout->addWidget(m_recentCombo);
    controlsRowLayout->addLayout(recentLayout);

    auto *openRecentBtn = new QPushButton(QStringLiteral("Open Recent"), rightWidget);
    openRecentBtn->setAccessibleName(QStringLiteral("Open Recent"));
    openRecentBtn->setAccessibleDescription(QStringLiteral("Open the selected recent file"));
    openRecentBtn->setToolTip(QStringLiteral("Open the selected recent file"));
    connect(openRecentBtn, &QPushButton::clicked, this, &MainWindow::onOpenRecent);
    controlsRowLayout->addWidget(openRecentBtn, 0, Qt::AlignBottom);
    controlsRowLayout->addStretch();
    rightLayout->addLayout(controlsRowLayout);

    // Tab Widget
    m_tabWidget = new QTabWidget(rightWidget);
    m_tabWidget->setAccessibleName(QStringLiteral("Table display mode"));
    m_previewWidget = new HtmlPreviewWidget(m_tabWidget);
    m_sourceEdit = new QPlainTextEdit(m_tabWidget);
    m_sourceEdit->setReadOnly(true);
    m_sourceEdit->setAccessibleName(QStringLiteral("HTML Source"));
    m_sourceEdit->setAccessibleDescription(QStringLiteral("Read-only HTML table source code"));
    QFont monoFont(QStringLiteral("monospace"));
    monoFont.setStyleHint(QFont::Monospace);
    m_sourceEdit->setFont(monoFont);

    m_tabWidget->addTab(m_previewWidget, QStringLiteral("Table Preview"));
    m_tabWidget->addTab(m_sourceEdit, QStringLiteral("HTML Source"));
    rightLayout->addWidget(m_tabWidget, 1);

    m_noteLabel = new QLabel(rightWidget);
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setAccessibleName(QStringLiteral("Table notice"));
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
    m_statusLabel->setAccessibleName(QStringLiteral("Status message"));
    statusRowLayout->addWidget(m_statusLabel, 1);

    m_revealButton = new QPushButton(QStringLiteral("Reveal file"), this);
    m_revealButton->setAccessibleName(QStringLiteral("Reveal file in file manager"));
    m_revealButton->setAccessibleDescription(QStringLiteral("Locate the exported file in your file manager"));
    m_revealButton->setToolTip(QStringLiteral("Locate the exported file in your file manager"));

    m_browserButton = new QPushButton(QStringLiteral("Open in Browser"), this);
    m_browserButton->setAccessibleName(QStringLiteral("Open exported file in browser"));
    m_browserButton->setAccessibleDescription(QStringLiteral("Open the exported file in your default web browser"));
    m_browserButton->setToolTip(QStringLiteral("Open the exported file in your default web browser"));

    connect(m_revealButton, &QPushButton::clicked, this, &MainWindow::onRevealLastExport);
    connect(m_browserButton, &QPushButton::clicked, this, &MainWindow::onOpenLastExportInBrowser);
    statusRowLayout->addWidget(m_revealButton);
    statusRowLayout->addWidget(m_browserButton);
    bottomLayout->addLayout(statusRowLayout);

    // Tab order for accessible keyboard navigation
    QWidget::setTabOrder(openBtn, batchBtn);
    QWidget::setTabOrder(batchBtn, sampleBtn);
    QWidget::setTabOrder(sampleBtn, refreshBtn);
    QWidget::setTabOrder(refreshBtn, copyBtn);
    QWidget::setTabOrder(copyBtn, exportBtn);
    QWidget::setTabOrder(exportBtn, m_cancelButton);
    QWidget::setTabOrder(m_cancelButton, settingsBtn);
    QWidget::setTabOrder(settingsBtn, m_presetsCombo);
    QWidget::setTabOrder(m_presetsCombo, loadPresetBtn);
    QWidget::setTabOrder(loadPresetBtn, deletePresetBtn);
    QWidget::setTabOrder(deletePresetBtn, m_savePresetEdit);
    QWidget::setTabOrder(m_savePresetEdit, savePresetBtn);
    QWidget::setTabOrder(savePresetBtn, styledBtn);
    QWidget::setTabOrder(styledBtn, unstyledBtn);
    QWidget::setTabOrder(unstyledBtn, m_delimiterCombo);
    QWidget::setTabOrder(m_delimiterCombo, m_recentCombo);
    QWidget::setTabOrder(m_recentCombo, openRecentBtn);
    QWidget::setTabOrder(openRecentBtn, m_tabWidget);

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
    m_openAction->setToolTip(QStringLiteral("Open CSV or TSV file (Ctrl+O)"));
    m_openAction->setStatusTip(QStringLiteral("Open a CSV or TSV file"));

    m_batchAction = fileMenu->addAction(QStringLiteral("&Batch Convert…"),
                                        QKeySequence(QStringLiteral("Ctrl+Shift+B")), this,
                                        &MainWindow::onBatch);
    m_batchAction->setToolTip(QStringLiteral("Batch convert CSV files (Ctrl+Shift+B)"));
    m_batchAction->setStatusTip(QStringLiteral("Batch convert multiple CSV files to HTML"));

    m_exportAction = fileMenu->addAction(QStringLiteral("&Export HTML…"), QKeySequence::Save, this,
                                         &MainWindow::onExport);
    m_exportAction->setToolTip(QStringLiteral("Export HTML (Ctrl+S)"));
    m_exportAction->setStatusTip(QStringLiteral("Export complete HTML table to file"));

    m_copyAction = fileMenu->addAction(QStringLiteral("&Copy HTML"),
                                       QKeySequence(QStringLiteral("Ctrl+Shift+C")), this,
                                       &MainWindow::onCopy);
    m_copyAction->setToolTip(QStringLiteral("Copy HTML Code (Ctrl+Shift+C)"));
    m_copyAction->setStatusTip(QStringLiteral("Copy complete HTML document to clipboard"));

    fileMenu->addSeparator();
    auto *exitAction = fileMenu->addAction(QStringLiteral("E&xit"), QKeySequence::Quit, this, &QWidget::close);
    exitAction->setToolTip(QStringLiteral("Exit YACHT"));

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    m_previewTabAction = viewMenu->addAction(QStringLiteral("Table &Preview"),
                                             QKeySequence(QStringLiteral("Ctrl+1")), this,
                                             [this] { switchTab(0); });
    m_previewTabAction->setToolTip(QStringLiteral("Switch to Table Preview tab (Ctrl+1)"));

    m_sourceTabAction = viewMenu->addAction(QStringLiteral("HTML &Source"),
                                            QKeySequence(QStringLiteral("Ctrl+2")), this,
                                            [this] { switchTab(1); });
    m_sourceTabAction->setToolTip(QStringLiteral("Switch to HTML Source tab (Ctrl+2)"));

    m_refreshAction = viewMenu->addAction(QStringLiteral("&Refresh"), this, &MainWindow::onRefresh);
    m_refreshAction->setShortcuts({QKeySequence::Refresh, QKeySequence(QStringLiteral("Ctrl+R"))});
    m_refreshAction->setToolTip(QStringLiteral("Refresh preview (F5 or Ctrl+R)"));
    m_refreshAction->setStatusTip(QStringLiteral("Refresh preview with current delimiter"));

    viewMenu->addSeparator();
    m_settingsAction = viewMenu->addAction(QStringLiteral("&Settings…"), this, &MainWindow::onSettings);
    m_settingsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
    m_settingsAction->setToolTip(QStringLiteral("Application settings (Ctrl+,)"));
    m_settingsAction->setStatusTip(QStringLiteral("Open settings and appearance preferences"));

    // Cancel action shortcut (Escape)
    auto *cancelAction = new QAction(this);
    cancelAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(cancelAction, &QAction::triggered, this, [this] {
        if (m_busy) onCancel();
    });
    addAction(cancelAction);

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
    auto *userGuideAction = helpMenu->addAction(QStringLiteral("&User Guide"), this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/tlolabs/yacht#using-yacht")));
    });
    userGuideAction->setShortcut(QKeySequence::HelpContents);
    userGuideAction->setToolTip(QStringLiteral("Open online User Guide (F1)"));

    auto *releasesAction = helpMenu->addAction(QStringLiteral("Download &Releases"), this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/tlolabs/yacht/releases")));
    });
    releasesAction->setToolTip(QStringLiteral("Open official releases page"));
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
    if (!busy && m_closeConfirmed) {
        QTimer::singleShot(0, this, &QWidget::close);
    }
}

void MainWindow::applyAppearance(const QString &appearance) {
    m_desktop->applyAppearance(appearance);
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
    if (!m_table || m_busy) return;
    m_previewDebounceTimer.stop();

    try {
        QJsonObject styleObj = currentStyle();
        int limit = m_preferences.previewRows;

        QJsonObject args;
        args[QStringLiteral("style")] = styleObj;
        args[QStringLiteral("limit")] = limit;

        m_cancelled = false;
        setBusy(true);
        TablePtr table = m_table;
        QJsonObject result = runCoreResponsive([this, table, args] {
            return table->call(QStringLiteral("preview"), args,
                               [this] { return m_cancelled.load(); }).toObject();
        });
        setBusy(false);

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
        m_cancelled = false;
        setBusy(false);
        m_statusLabel->setText(QStringLiteral("Cancelled"));
    } catch (const std::exception &e) {
        setBusy(false);
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
        const QString delimiter = m_delimiter;
        TablePtr nextTable = runCoreResponsive([this, path, delimiter] {
            return YachtCore::read(path, delimiter, [this] { return m_cancelled.load(); });
        });
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
        m_cancelled = false;
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
        TablePtr table = m_table;
        QJsonValue htmlVal = runCoreResponsive([this, table, args] {
            return table->call(QStringLiteral("html"), args,
                               [this] { return m_cancelled.load(); });
        });
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
        TablePtr table = m_table;
        runCoreResponsive([this, table, args] {
            return table->call(QStringLiteral("export"), args,
                               [this] { return m_cancelled.load(); });
        });
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
        setBusy(false);
        onPerformRender();
        return;
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

            QJsonArray batchRes = runCoreResponsive([this, args] {
                return YachtCore::call(QStringLiteral("batch"), args,
                                       [this] { return m_cancelled.load(); }).toArray();
            });
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
    if (m_busy) {
        if (m_closeConfirmed || mayClose()) {
            m_closeConfirmed = true;
            cancelActiveWork();
        }
        event->ignore();
        return;
    }
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
