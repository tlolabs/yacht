#include "SettingsDialog.h"
#include "AppIdentity.h"
#include "YachtCore.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Yacht {

SettingsDialog::SettingsDialog(Preferences &prefs, QWidget *parent)
    : QDialog(parent), m_prefs(prefs) {
    setWindowTitle(QStringLiteral("Settings"));
    setMinimumWidth(400);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    m_rememberCheckBox = new QCheckBox(QStringLiteral("Remember last-used table style"), this);
    m_rememberCheckBox->setAccessibleName(QStringLiteral("Remember last-used table style"));
    m_rememberCheckBox->setAccessibleDescription(QStringLiteral("Automatically save and restore custom style configuration"));
    m_rememberCheckBox->setToolTip(QStringLiteral("Automatically save and restore custom style configuration"));
    m_rememberCheckBox->setChecked(m_prefs.rememberStyle);
    mainLayout->addWidget(m_rememberCheckBox);

    auto *rowsLabel = new QLabel(QStringLiteral("Maximum preview rows"), this);
    mainLayout->addWidget(rowsLabel);

    m_previewRowsCombo = new QComboBox(this);
    m_previewRowsCombo->setAccessibleName(QStringLiteral("Maximum preview rows"));
    m_previewRowsCombo->setAccessibleDescription(QStringLiteral("Set maximum row count for table preview"));
    m_previewRowsCombo->setToolTip(QStringLiteral("Maximum number of rows to display in preview table"));
    rowsLabel->setBuddy(m_previewRowsCombo);
    m_previewRowsCombo->addItem(QStringLiteral("50"), 50);
    m_previewRowsCombo->addItem(QStringLiteral("200"), 200);
    m_previewRowsCombo->addItem(QStringLiteral("1000"), 1000);
    int rowIdx = m_previewRowsCombo->findData(m_prefs.previewRows);
    if (rowIdx >= 0) m_previewRowsCombo->setCurrentIndex(rowIdx);
    mainLayout->addWidget(m_previewRowsCombo);

    auto *appLabel = new QLabel(QStringLiteral("Appearance"), this);
    mainLayout->addWidget(appLabel);

    m_appearanceCombo = new QComboBox(this);
    m_appearanceCombo->setAccessibleName(QStringLiteral("Appearance"));
    m_appearanceCombo->setAccessibleDescription(QStringLiteral("Set application color theme"));
    m_appearanceCombo->setToolTip(QStringLiteral("Choose System, Light, or Dark appearance"));
    appLabel->setBuddy(m_appearanceCombo);
    m_appearanceCombo->addItem(QStringLiteral("System"));
    m_appearanceCombo->addItem(QStringLiteral("Light"));
    m_appearanceCombo->addItem(QStringLiteral("Dark"));
    int appIdx = m_appearanceCombo->findText(m_prefs.appearance);
    if (appIdx >= 0) m_appearanceCombo->setCurrentIndex(appIdx);
    mainLayout->addWidget(m_appearanceCombo);

    m_clearRecentButton = new QPushButton(QStringLiteral("Clear Recent Files"), this);
    m_clearRecentButton->setAccessibleName(QStringLiteral("Clear Recent Files"));
    m_clearRecentButton->setAccessibleDescription(QStringLiteral("Clear recent files history"));
    if (m_prefs.recent.isEmpty()) {
        m_clearRecentButton->setEnabled(false);
        m_clearRecentButton->setToolTip(QStringLiteral("No recent files to clear"));
    } else {
        m_clearRecentButton->setToolTip(QStringLiteral("Remove all items from the recent files menu"));
    }
    connect(m_clearRecentButton, &QPushButton::clicked, this, &SettingsDialog::onClearRecent);
    mainLayout->addWidget(m_clearRecentButton);

    QString version = QStringLiteral("2.1.2");
    try {
        version = YachtCore::call(QStringLiteral("info")).toObject().value(QStringLiteral("version")).toString(version);
    } catch (...) {}

    auto *infoLabel = new QLabel(AppIdentity::title() + QStringLiteral(" ") + version +
                                 QStringLiteral(" · GPLv3"), this);
    mainLayout->addWidget(infoLabel);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Save)->setDefault(true);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onSave);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setTabOrder(m_rememberCheckBox, m_previewRowsCombo);
    setTabOrder(m_previewRowsCombo, m_appearanceCombo);
    setTabOrder(m_appearanceCombo, m_clearRecentButton);
    setTabOrder(m_clearRecentButton, buttonBox);
}

void SettingsDialog::onClearRecent() {
    m_clearRecent = true;
    m_clearRecentButton->setText(QStringLiteral("Recent files will be cleared on Save"));
    m_clearRecentButton->setEnabled(false);
}

void SettingsDialog::onSave() {
    m_prefs.rememberStyle = m_rememberCheckBox->isChecked();
    m_prefs.previewRows = m_previewRowsCombo->currentData().toInt();
    m_prefs.appearance = m_appearanceCombo->currentText();
    if (m_clearRecent) {
        m_prefs.recent.clear();
    }
    accept();
}

} // namespace Yacht
