#include "BatchDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Yacht {

BatchDialog::BatchDialog(const QStringList &paths, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Batch Convert CSVs"));
    resize(550, 400);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    auto *fileList = new QTextEdit(this);
    fileList->setReadOnly(true);
    fileList->setPlainText(paths.join(QLatin1Char('\n')));
    fileList->setAccessibleName(QStringLiteral("Files to convert"));
    fileList->setAccessibleDescription(QStringLiteral("List of selected files to convert to HTML"));
    fileList->setToolTip(QStringLiteral("Selected CSV files to be converted"));
    mainLayout->addWidget(fileList);

    m_overwriteCheckBox = new QCheckBox(QStringLiteral("Replace existing HTML files"), this);
    m_overwriteCheckBox->setAccessibleName(QStringLiteral("Replace existing HTML files"));
    m_overwriteCheckBox->setAccessibleDescription(QStringLiteral("Overwrite existing HTML files with the same name"));
    m_overwriteCheckBox->setToolTip(QStringLiteral("If checked, existing HTML files with matching names will be overwritten"));
    mainLayout->addWidget(m_overwriteCheckBox);

    auto *buttonBox = new QDialogButtonBox(this);
    auto *convertBtn = buttonBox->addButton(QStringLiteral("Convert"), QDialogButtonBox::AcceptRole);
    buttonBox->addButton(QDialogButtonBox::Cancel);
    convertBtn->setDefault(true);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setTabOrder(fileList, m_overwriteCheckBox);
    setTabOrder(m_overwriteCheckBox, buttonBox);
}

bool BatchDialog::overwrite() const {
    return m_overwriteCheckBox && m_overwriteCheckBox->isChecked();
}

} // namespace Yacht
