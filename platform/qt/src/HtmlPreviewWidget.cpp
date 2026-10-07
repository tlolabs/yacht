#include "HtmlPreviewWidget.h"

#include <QRegularExpression>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

namespace Yacht {

HtmlPreviewWidget::HtmlPreviewWidget(QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_browser = new QTextBrowser(this);
    m_browser->setAccessibleName(QStringLiteral("Generated table preview"));
    m_browser->setOpenExternalLinks(false);
    layout->addWidget(m_browser);
}

void HtmlPreviewWidget::setHtml(const QString &html) {
    m_lastHtml = html;
    m_browser->setHtml(html);
    m_navigationComplete = true;
}

void HtmlPreviewWidget::clear() {
    m_lastHtml.clear();
    m_browser->clear();
    m_navigationComplete = false;
}

QString HtmlPreviewWidget::inspectSmokeDocument() const {
    if (m_lastHtml.isEmpty()) {
        return QStringLiteral("0");
    }

    // Verify tbody contains 2 rows and first td contains escaped <script>
    int tbodyStart = m_lastHtml.indexOf(QStringLiteral("<tbody>"));
    int tbodyEnd = m_lastHtml.indexOf(QStringLiteral("</tbody>"));
    if (tbodyStart < 0 || tbodyEnd <= tbodyStart) {
        return QStringLiteral("0");
    }

    QString tbody = m_lastHtml.mid(tbodyStart, tbodyEnd - tbodyStart);
    int trCount = tbody.count(QStringLiteral("<tr>"));
    if (trCount != 2) {
        return QStringLiteral("0");
    }

    // Verify unescaped <script> tag does not exist
    if (m_lastHtml.contains(QStringLiteral("<script>")) || m_lastHtml.contains(QStringLiteral("</script>"))) {
        return QStringLiteral("0");
    }

    // Verify browser renders the text literally as "<script>"
    QString text = m_browser->toPlainText();
    if (!text.contains(QStringLiteral("<script>"))) {
        return QStringLiteral("0");
    }

    return QStringLiteral("1");
}

} // namespace Yacht
