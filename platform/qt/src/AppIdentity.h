#pragma once

#include <QString>

namespace Yacht {

class AppIdentity {
public:
    static bool isInternal();
    static QString title();
    static QString dataDirectory();
    static QString presetPath();
    static QString channelName();
};

} // namespace Yacht
