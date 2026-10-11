#pragma once

#include <QJsonObject>
#include <QString>

namespace Yacht {

class Presets {
public:
    static QJsonObject load();
    static void save(const QJsonObject &presets);
};

} // namespace Yacht
