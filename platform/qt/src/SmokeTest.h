#pragma once

#include "MainWindow.h"

#include <QString>

namespace Yacht {

class SmokeTest {
public:
    static int run(MainWindow &window, const QString &directory);
};

} // namespace Yacht
