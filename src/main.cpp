#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("SpriteStitcher");
    QApplication::setApplicationDisplayName("SpriteStitcher");
    QApplication::setApplicationVersion(APP_VERSION);
    QApplication::setOrganizationName("JesteraceTools");

    MainWindow w;
    w.show();
    return app.exec();
}
