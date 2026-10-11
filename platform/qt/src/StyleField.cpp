#include "StyleField.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Yacht {

StyleField::StyleField(const QString &key, const QString &label,
                       const QJsonValue &initialValue, QWidget *parent)
    : QWidget(parent), m_key(key), m_label(label) {
    if (m_key == QStringLiteral("border_style")) {
        m_choices = {QStringLiteral("solid"),   QStringLiteral("dashed"),
                     QStringLiteral("dotted"),  QStringLiteral("double"),
                     QStringLiteral("none"),    QStringLiteral("hidden"),
                     QStringLiteral("groove"),  QStringLiteral("ridge"),
                     QStringLiteral("inset"),   QStringLiteral("outset")};
    } else if (m_key == QStringLiteral("border_collapse")) {
        m_choices = {QStringLiteral("collapse"), QStringLiteral("separate")};
    }

    m_isChoice = !m_choices.isEmpty();
    m_isFlag = initialValue.isBool();
    m_isNumber = initialValue.isDouble();
    m_isColor = m_key.endsWith(QStringLiteral("_bg")) || m_key.endsWith(QStringLiteral("_color"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(4);

    auto *lbl = new QLabel(m_label, this);
    layout->addWidget(lbl);

    if (m_isFlag) {
        m_checkBox = new QCheckBox(QStringLiteral("Enabled"), this);
        m_checkBox->setAccessibleName(m_label);
        m_checkBox->setAccessibleDescription(QStringLiteral("Toggle %1").arg(m_label));
        m_checkBox->setToolTip(m_label);
        lbl->setBuddy(m_checkBox);
        m_checkBox->setChecked(initialValue.toBool());
        layout->addWidget(m_checkBox);
        connect(m_checkBox, &QCheckBox::toggled, this, &StyleField::valueChanged);
    } else if (m_isChoice) {
        m_comboBox = new QComboBox(this);
        m_comboBox->setAccessibleName(m_label);
        m_comboBox->setAccessibleDescription(QStringLiteral("Select %1").arg(m_label));
        m_comboBox->setToolTip(m_label);
        lbl->setBuddy(m_comboBox);
        m_comboBox->addItems(m_choices);
        int idx = m_comboBox->findText(initialValue.toString());
        if (idx >= 0) {
            m_comboBox->setCurrentIndex(idx);
        }
        layout->addWidget(m_comboBox);
        connect(m_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &StyleField::valueChanged);
    } else {
        auto *inputLayout = new QHBoxLayout();
        inputLayout->setContentsMargins(0, 0, 0, 0);
        inputLayout->setSpacing(4);

        m_lineEdit = new QLineEdit(this);
        m_lineEdit->setAccessibleName(m_label);
        lbl->setBuddy(m_lineEdit);
        if (m_isNumber) {
            m_lineEdit->setText(QString::number(initialValue.toInteger()));
            m_lineEdit->setPlaceholderText(QStringLiteral("e.g. 0"));
            m_lineEdit->setAccessibleDescription(QStringLiteral("Numeric value for %1").arg(m_label));
        } else if (m_isColor) {
            m_lineEdit->setText(initialValue.toString());
            m_lineEdit->setPlaceholderText(QStringLiteral("#rrggbb or color name"));
            m_lineEdit->setAccessibleDescription(QStringLiteral("Hex color or name for %1").arg(m_label));
        } else {
            m_lineEdit->setText(initialValue.toString());
            m_lineEdit->setAccessibleDescription(QStringLiteral("Value for %1").arg(m_label));
        }
        m_lineEdit->setToolTip(m_label);
        inputLayout->addWidget(m_lineEdit);
        connect(m_lineEdit, &QLineEdit::textChanged, this, &StyleField::valueChanged);

        if (m_isColor) {
            m_colorButton = new QPushButton(QStringLiteral("Choose color…"), this);
            m_colorButton->setAccessibleName(QStringLiteral("Choose %1").arg(m_label));
            m_colorButton->setAccessibleDescription(QStringLiteral("Open color picker dialog for %1").arg(m_label));
            m_colorButton->setToolTip(QStringLiteral("Choose %1 color").arg(m_label));
            inputLayout->addWidget(m_colorButton);
            connect(m_colorButton, &QPushButton::clicked, this, &StyleField::onPickColor);
        }

        layout->addLayout(inputLayout);
    }
}

QJsonValue StyleField::value() const {
    if (m_isFlag) {
        return QJsonValue(m_checkBox->isChecked());
    }
    if (m_isChoice) {
        return QJsonValue(m_comboBox->currentText());
    }
    if (m_isNumber) {
        bool ok = false;
        qint64 num = m_lineEdit->text().toLongLong(&ok);
        if (ok) {
            return QJsonValue(num);
        }
        // If non-numeric text was typed, preserve it as a string so Rust core validation fails
        return QJsonValue(m_lineEdit->text());
    }
    return QJsonValue(m_lineEdit ? m_lineEdit->text() : QString());
}

void StyleField::setValue(const QJsonValue &val) {
    if (m_isFlag && m_checkBox) {
        m_checkBox->setChecked(val.toBool());
    } else if (m_isChoice && m_comboBox) {
        int idx = m_comboBox->findText(val.toString());
        if (idx >= 0) m_comboBox->setCurrentIndex(idx);
    } else if (m_lineEdit) {
        if (m_isNumber && val.isDouble()) {
            m_lineEdit->setText(QString::number(val.toInteger()));
        } else {
            m_lineEdit->setText(val.toString());
        }
    }
}

void StyleField::setFieldText(const QString &text) {
    if (m_lineEdit) {
        m_lineEdit->setText(text);
    }
}

QString StyleField::fieldText() const {
    if (m_lineEdit) {
        return m_lineEdit->text();
    }
    if (m_isChoice && m_comboBox) {
        return m_comboBox->currentText();
    }
    return {};
}

void StyleField::onPickColor() {
    QColor initial = QColor::fromString(fieldText());
    if (!initial.isValid()) {
        initial = Qt::black;
    }
    QColor color = QColorDialog::getColor(initial, this, m_label,
                                          QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if (!color.isValid()) {
        // Try native if needed or cancel
        return;
    }
    setFieldText(color.name(QColor::HexRgb));
}

} // namespace Yacht
