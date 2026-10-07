#pragma once

#include "HtmlPreviewWidget.h"
#include "Preferences.h"
#include "StyleField.h"
#include "YachtCore.h"

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <atomic>
#include <vector>

class QAction;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QTabWidget;

namespace Yacht {

class IDesktopServices;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(std::shared_ptr<IDesktopServices> desktop = nullptr,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    void initialize();
    bool isBusy() const { return m_busy; }

    void loadFile(const QString &path, bool infer = true);
    void receive(const QStringList &paths, bool batch = false);
    void setStyle(const QJsonObject &style);
    QJsonObject currentStyle() const;

    void exportTo(const QString &path, bool overwrite);
    void cancelActiveWork();
    void checkUpdates(bool automatic);

    HtmlPreviewWidget *previewWidget() const { return m_previewWidget; }
    QPlainTextEdit *sourceEdit() const { return m_sourceEdit; }
    QString statusText() const;
    QString summaryText() const;
    QString noteText() const;
    QString delimiter() const;
    void setDelimiter(const QString &delim);

    const Preferences &preferences() const { return m_preferences; }
    Preferences &preferences() { return m_preferences; }
    void savePreferences();

    const std::vector<StyleField *> &styleFields() const { return m_styleFields; }
    StyleField *findField(const QString &key) const;

    QString currentPresetName() const;
    void setPresetNameInput(const QString &name);
    QStringList presetNames() const;

    void selectPreset(const QString &name);
    void loadSelectedPreset();
    void saveCurrentPreset();
    void deleteSelectedPreset();

    void copyHtmlToClipboard();
    void batchConvert(const QStringList &paths);

    void switchTab(int index);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

public slots:
    void onOpen();
    void onBatch();
    void onSample();
    void onRefresh();
    void onCopy();
    void onExport();
    void onCancel();
    void onSettings();
    void onResetStyled();
    void onResetUnstyled();
    void onLoadPreset();
    void onSavePreset();
    void onDeletePreset();
    void onOpenRecent();
    void onRevealLastExport();
    void onOpenLastExportInBrowser();
    void onStyleFieldChanged();
    void onPerformRender();
    void onDelimiterChanged(int index);

private:
    void setupUi();
    void setupActions();
    void syncPresets();
    void syncRecent();
    void setBusy(bool busy);
    void applyAppearance(const QString &appearance);
    bool mayClose();

    std::shared_ptr<IDesktopServices> m_desktop;
    Preferences m_preferences;
    TablePtr m_table;
    QJsonObject m_presets;
    QString m_sourcePath;
    QString m_lastExport;
    QString m_delimiter{QStringLiteral("comma")};
    bool m_busy{false};
    std::atomic<bool> m_cancelled{false};
    bool m_closeConfirmed{false};

    QTimer m_previewDebounceTimer;
    QTimer m_updateTimer;

    // Controls
    QAction *m_openAction{nullptr};
    QAction *m_batchAction{nullptr};
    QAction *m_exportAction{nullptr};
    QAction *m_copyAction{nullptr};
    QAction *m_refreshAction{nullptr};
    QAction *m_previewTabAction{nullptr};
    QAction *m_sourceTabAction{nullptr};
    QAction *m_settingsAction{nullptr};

    QPushButton *m_cancelButton{nullptr};
    QPushButton *m_revealButton{nullptr};
    QPushButton *m_browserButton{nullptr};

    QWidget *m_sidebarWidget{nullptr};
    QComboBox *m_presetsCombo{nullptr};
    QLineEdit *m_savePresetEdit{nullptr};
    std::vector<StyleField *> m_styleFields;

    QLabel *m_summaryLabel{nullptr};
    QComboBox *m_delimiterCombo{nullptr};
    QComboBox *m_recentCombo{nullptr};
    QTabWidget *m_tabWidget{nullptr};
    HtmlPreviewWidget *m_previewWidget{nullptr};
    QPlainTextEdit *m_sourceEdit{nullptr};
    QLabel *m_noteLabel{nullptr};

    QProgressBar *m_progressBar{nullptr};
    QLabel *m_statusLabel{nullptr};
};

} // namespace Yacht
