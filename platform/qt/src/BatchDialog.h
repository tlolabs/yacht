#pragma once

#include <QDialog>
#include <QStringList>

class QCheckBox;

namespace Yacht {

class BatchDialog : public QDialog {
    Q_OBJECT
public:
    BatchDialog(const QStringList &paths, QWidget *parent = nullptr);

    bool overwrite() const;

private:
    QCheckBox *m_overwriteCheckBox{nullptr};
};

} // namespace Yacht
