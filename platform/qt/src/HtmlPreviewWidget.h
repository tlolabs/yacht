#pragma once

#include <QWidget>

class QTextBrowser;

namespace Yacht {

class HtmlPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit HtmlPreviewWidget(QWidget *parent = nullptr);

    void setHtml(const QString &html);
    void clear();

    bool isNavigationComplete() const { return m_navigationComplete; }
    QString inspectSmokeDocument() const;

private:
    QTextBrowser *m_browser{nullptr};
    bool m_navigationComplete{false};
    QString m_lastHtml;
};

} // namespace Yacht
