#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

namespace Yacht {

class StyleField : public QWidget {
    Q_OBJECT
public:
    StyleField(const QString &key, const QString &label, const QJsonValue &initialValue,
               QWidget *parent = nullptr);

    const QString &key() const { return m_key; }
    const QString &label() const { return m_label; }

    QJsonValue value() const;
    void setValue(const QJsonValue &val);

    void setFieldText(const QString &text);
    QString fieldText() const;

signals:
    void valueChanged();

private slots:
    void onPickColor();

private:
    QString m_key;
    QString m_label;
    QStringList m_choices;
    bool m_isFlag{false};
    bool m_isNumber{false};
    bool m_isColor{false};
    bool m_isChoice{false};

    QLineEdit *m_lineEdit{nullptr};
    QCheckBox *m_checkBox{nullptr};
    QComboBox *m_comboBox{nullptr};
    QPushButton *m_colorButton{nullptr};
};

} // namespace Yacht
