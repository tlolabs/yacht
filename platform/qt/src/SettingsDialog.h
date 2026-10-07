#pragma once

#include "Preferences.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QPushButton;

namespace Yacht {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(Preferences &prefs, QWidget *parent = nullptr);

private slots:
    void onClearRecent();
    void onSave();

private:
    Preferences &m_prefs;
    QCheckBox *m_rememberCheckBox{nullptr};
    QComboBox *m_previewRowsCombo{nullptr};
    QComboBox *m_appearanceCombo{nullptr};
    QPushButton *m_clearRecentButton{nullptr};
    bool m_clearRecent{false};
};

} // namespace Yacht
