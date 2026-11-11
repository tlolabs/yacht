#pragma once

#include "MainWindow.h"

#include <QString>

class QElapsedTimer;

namespace Yacht {

class SmokeTest {
public:
    static int run(MainWindow &window, const QString &directory,
                   const QElapsedTimer *launchTimer = nullptr);
};

} // namespace Yacht
