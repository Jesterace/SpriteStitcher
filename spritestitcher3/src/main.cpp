#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SpriteStitcher3"));
    QApplication::setApplicationDisplayName(QStringLiteral("SpriteStitcher 3 Prototype"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setOrganizationName(QStringLiteral("JesteraceTools"));

    MainWindow window;
    window.show();
    return app.exec();
}
