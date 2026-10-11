#pragma once

#include "Preferences.h"

#include <QString>
#include <QStringList>
#include <memory>
#include <optional>

class QWidget;

namespace Yacht {

class IDesktopServices {
public:
    virtual ~IDesktopServices() = default;
    virtual QStringList openFiles() = 0;
    virtual QString saveFile(const QString &suggestedName) = 0;
    virtual bool confirm(const QString &title, const QString &message,
                         const QString &acceptText = QStringLiteral("OK")) = 0;
    virtual void showMessage(const QString &title, const QString &message) = 0;
    virtual std::optional<bool> reviewBatch(const QStringList &paths) = 0;
    virtual bool settings(Preferences &preferences) = 0;
    virtual void setClipboard(const QString &text) = 0;
    virtual QString clipboard() const = 0;
    virtual void reveal(const QString &path) = 0;
    virtual void open(const QString &pathOrUrl) = 0;
    virtual QString pickColor(const QString &label, const QString &initialValue) = 0;
    virtual void applyAppearance(const QString &appearance) = 0;
};

class QtDesktopServices : public IDesktopServices {
public:
    explicit QtDesktopServices(QWidget *owner);

    QStringList openFiles() override;
    QString saveFile(const QString &suggestedName) override;
    bool confirm(const QString &title, const QString &message,
                 const QString &acceptText = QStringLiteral("OK")) override;
    void showMessage(const QString &title, const QString &message) override;
    std::optional<bool> reviewBatch(const QStringList &paths) override;
    bool settings(Preferences &preferences) override;
    void setClipboard(const QString &text) override;
    QString clipboard() const override;
    void reveal(const QString &path) override;
    void open(const QString &pathOrUrl) override;
    QString pickColor(const QString &label, const QString &initialValue) override;
    void applyAppearance(const QString &appearance) override;

private:
    QWidget *m_owner{nullptr};
};

} // namespace Yacht
