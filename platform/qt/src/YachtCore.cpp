#include "YachtCore.h"

#include <QJsonDocument>
#include <QJsonObject>

extern "C" {
char *yacht_request(const char *request, bool (*cancelled)(void *), void *context);
void yacht_free(char *response);
}

namespace Yacht {

namespace {

struct CancelContext {
    std::function<bool()> check;
};

extern "C" bool yacht_cancel_trampoline(void *context) {
    if (!context) return false;
    auto *ctx = static_cast<CancelContext *>(context);
    return ctx->check ? ctx->check() : false;
}

} // namespace

Table::Table(const QJsonObject &metadata)
    : m_metadata(metadata),
      m_handle(static_cast<quint64>(metadata.value(QStringLiteral("handle")).toInteger())) {}

Table::~Table() {
    release();
}

quint64 Table::handle() const {
    return m_handle;
}

QJsonObject Table::metadata() const {
    return m_metadata;
}

void Table::release() {
    std::lock_guard<std::mutex> lock(m_gate);
    if (m_released) {
        return;
    }
    m_released = true;
    try {
        QJsonObject args;
        args[QStringLiteral("handle")] = static_cast<qint64>(m_handle);
        YachtCore::call(QStringLiteral("release"), args);
    } catch (...) {
        // Suppress during process shutdown or destruction
    }
}

QJsonValue Table::call(const QString &op, const QJsonObject &args,
                       std::function<bool()> isCancelled) {
    std::lock_guard<std::mutex> lock(m_gate);
    if (m_released) {
        throw std::runtime_error("Table has already been released.");
    }
    QJsonObject request = args;
    request[QStringLiteral("handle")] = static_cast<qint64>(m_handle);
    return YachtCore::call(op, request, isCancelled);
}

QJsonValue YachtCore::call(const QString &operation, const QJsonObject &arguments,
                           std::function<bool()> isCancelled) {
    QJsonObject request = arguments;
    request[QStringLiteral("op")] = operation;
    request[QStringLiteral("version")] = 1;

    QByteArray jsonBytes = QJsonDocument(request).toJson(QJsonDocument::Compact);

    CancelContext ctx{isCancelled};
    bool (*cancelCallback)(void *) = isCancelled ? yacht_cancel_trampoline : nullptr;
    void *ctxPtr = isCancelled ? &ctx : nullptr;

    char *rawResponse = yacht_request(jsonBytes.constData(), cancelCallback, ctxPtr);
    if (!rawResponse) {
        throw std::runtime_error("The Rust core returned no response.");
    }

    QByteArray responseBytes(rawResponse);
    yacht_free(rawResponse);

    QJsonParseError parseError;
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !responseDoc.isObject()) {
        throw std::runtime_error("Invalid JSON response from Rust core: " +
                                 parseError.errorString().toStdString());
    }

    QJsonObject responseObj = responseDoc.object();
    if (responseObj.contains(QStringLiteral("error"))) {
        QJsonObject err = responseObj.value(QStringLiteral("error")).toObject();
        QString code = err.value(QStringLiteral("code")).toString();
        QString message = err.value(QStringLiteral("message")).toString();
        if (code == QStringLiteral("cancelled")) {
            throw YachtCancelledException();
        }
        throw YachtCoreException(code, message);
    }

    return responseObj.value(QStringLiteral("ok"));
}

QJsonObject YachtCore::style(const QString &operation) {
    return call(operation).toObject();
}

TablePtr YachtCore::read(const QString &path, const QString &delimiter,
                         std::function<bool()> isCancelled) {
    QJsonObject args;
    args[QStringLiteral("path")] = path;
    args[QStringLiteral("delimiter")] = delimiter;
    QJsonObject meta = call(QStringLiteral("read"), args, isCancelled).toObject();
    return std::make_shared<Table>(meta);
}

TablePtr YachtCore::sample() {
    QJsonObject meta = call(QStringLiteral("sample")).toObject();
    return std::make_shared<Table>(meta);
}

} // namespace Yacht
