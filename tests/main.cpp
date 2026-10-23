#include <QGuiApplication>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QtQml>

#include "pin.h"

int runCorbelTests(int argc, char **argv);

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    // Settings (the last project, the last folder) go to a test location,
    // not the user's own.
    QStandardPaths::setTestModeEnabled(true);
    QQuickStyle::setStyle("Material");
    qmlRegisterType<PinItem>("Corbel", 1, 0, "Pin");
    int failed = runCorbelTests(argc, argv);
    return failed;
}
