#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace Yacht {

class YachtCoreException : public std::runtime_error {
public:
    YachtCoreException(const QString &code, const QString &message)
        : std::runtime_error(message.toStdString()), m_code(code), m_message(message) {}

    const QString &code() const { return m_code; }
    const QString &message() const { return m_message; }

private:
    QString m_code;
    QString m_message;
};

class YachtCancelledException : public std::runtime_error {
public:
    YachtCancelledException() : std::runtime_error("The operation was cancelled.") {}
};

class Table {
public:
    explicit Table(const QJsonObject &metadata);
    ~Table();

    Table(const Table &) = delete;
    Table &operator=(const Table &) = delete;

    quint64 handle() const;
    QJsonObject metadata() const;

    QJsonValue call(const QString &op, const QJsonObject &args = QJsonObject(),
                    std::function<bool()> isCancelled = nullptr);
    void release();

private:
    QJsonObject m_metadata;
    quint64 m_handle{0};
    mutable std::mutex m_gate;
    bool m_released{false};
};

using TablePtr = std::shared_ptr<Table>;

class YachtCore {
public:
    static QJsonValue call(const QString &operation, const QJsonObject &arguments = QJsonObject(),
                           std::function<bool()> isCancelled = nullptr);

    static QJsonObject style(const QString &operation = QStringLiteral("defaults"));

    static TablePtr read(const QString &path, const QString &delimiter,
                         std::function<bool()> isCancelled = nullptr);

    static TablePtr sample();
};

} // namespace Yacht
