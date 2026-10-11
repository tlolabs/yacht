#include "DesktopServices.h"
#include "BatchDialog.h"
#include "SettingsDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QUrl>

namespace Yacht {

QtDesktopServices::QtDesktopServices(QWidget *owner) : m_owner(owner) {}

QStringList QtDesktopServices::openFiles() {
    return QFileDialog::getOpenFileNames(
        m_owner, QStringLiteral("Open CSV files"), QString(),
        QStringLiteral("CSV / TSV / Text (*.csv *.tsv *.txt);;All Files (*.*)"));
}

QString QtDesktopServices::saveFile(const QString &suggestedName) {
    return QFileDialog::getSaveFileName(
        m_owner, QStringLiteral("Export HTML"), suggestedName, QStringLiteral("HTML (*.html)"));
}

bool QtDesktopServices::confirm(const QString &title, const QString &message,
                                const QString &acceptText) {
    QMessageBox box(m_owner);
    box.setWindowTitle(title);
    box.setText(message);
    QAbstractButton *okBtn = box.addButton(acceptText, QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(qobject_cast<QPushButton *>(okBtn));
    box.exec();
    return box.clickedButton() == okBtn;
}

void QtDesktopServices::showMessage(const QString &title, const QString &message) {
    QMessageBox::information(m_owner, title, message);
}

std::optional<bool> QtDesktopServices::reviewBatch(const QStringList &paths) {
    BatchDialog dialog(paths, m_owner);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.overwrite();
    }
    return std::nullopt;
}

bool QtDesktopServices::settings(Preferences &preferences) {
    SettingsDialog dialog(preferences, m_owner);
    return dialog.exec() == QDialog::Accepted;
}

void QtDesktopServices::setClipboard(const QString &text) {
    QApplication::clipboard()->setText(text);
}

QString QtDesktopServices::clipboard() const {
    return QApplication::clipboard()->text();
}

void QtDesktopServices::reveal(const QString &path) {
    if (path.isEmpty()) return;
#if defined(Q_OS_WIN)
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), path});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("/usr/bin/open"), {QStringLiteral("-R"), path});
#else
    QProcess::startDetached(QStringLiteral("xdg-open"), {QFileInfo(path).path()});
#endif
}

void QtDesktopServices::open(const QString &pathOrUrl) {
    if (pathOrUrl.isEmpty()) return;
    if (pathOrUrl.startsWith(QStringLiteral("http://")) || pathOrUrl.startsWith(QStringLiteral("https://"))) {
        QDesktopServices::openUrl(QUrl(pathOrUrl));
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(pathOrUrl));
    }
}

QString QtDesktopServices::pickColor(const QString &label, const QString &initialValue) {
    QColor initial = QColor::fromString(initialValue);
    if (!initial.isValid()) {
        initial = Qt::black;
    }
    QColor color = QColorDialog::getColor(initial, m_owner, label,
                                          QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
    if (!color.isValid()) {
        return {};
    }
    return color.name(QColor::HexRgb);
}

void QtDesktopServices::applyAppearance(const QString &appearance) {
    // Restore the platform style before changing schemes. Otherwise switching
    // back from a Fusion fallback leaves System looking like a forced theme.
    static const QString systemStyle = QApplication::style()->objectName();
    if (!systemStyle.isEmpty() && QApplication::style()->objectName() != systemStyle) {
        if (QStyle *native = QStyleFactory::create(systemStyle)) {
            QApplication::setStyle(native);
        }
    }
    QApplication::setPalette(QPalette());

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    Qt::ColorScheme scheme = Qt::ColorScheme::Unknown;
    if (appearance == QStringLiteral("Dark")) scheme = Qt::ColorScheme::Dark;
    if (appearance == QStringLiteral("Light")) scheme = Qt::ColorScheme::Light;
    QGuiApplication::styleHints()->setColorScheme(scheme);
    if (scheme == Qt::ColorScheme::Unknown) return;
    const int windowLightness = QApplication::palette().color(QPalette::Window).lightness();
    if ((scheme == Qt::ColorScheme::Dark && windowLightness < 128) ||
        (scheme == Qt::ColorScheme::Light && windowLightness >= 128)) {
        return;
    }
#else
    if (appearance == QStringLiteral("System")) return;
#endif

    // Qt 6.4 and platforms that cannot override their native color scheme use
    // Fusion for explicit Light/Dark. System always restores the native style.
    if (appearance == QStringLiteral("Dark")) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
        darkPalette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::ToolTipBase, QColor(40, 40, 40));
        darkPalette.setColor(QPalette::ToolTipText, QColor(240, 240, 240));
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(64, 158, 255));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);

        darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
        darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(128, 128, 128));
        darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(140, 140, 140));

        QApplication::setPalette(darkPalette);
    } else if (appearance == QStringLiteral("Light")) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(QApplication::style()->standardPalette());
    }
}

} // namespace Yacht
