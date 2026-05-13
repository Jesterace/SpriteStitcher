#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("SpriteStitchCPP");
    QApplication::setApplicationDisplayName("SpriteStitch C++");
    QApplication::setApplicationVersion(APP_VERSION);
    QApplication::setOrganizationName("JesteraceTools");

    MainWindow w;
    w.show();
    return app.exec();
}
