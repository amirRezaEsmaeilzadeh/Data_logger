#include "mainwindow.h"
#include <QApplication>
#include <QSplashScreen>
#include <QTimer>
#include <QPixmap>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Setup splash screan
    QPixmap pixmap(":/logo.png");
    QSplashScreen splash(pixmap);
    splash.show();
    splash.showMessage("Initializing Network Sockets...", Qt::AlignBottom | Qt::AlignCenter, Qt::white);

    // Force to draw splash
    a.processEvents();

    // Load connection in bg
    MainWindow w;

    // Close splash screan
    QTimer::singleShot(2000, &splash, &QSplashScreen::close);
    QTimer::singleShot(2000, &w, &MainWindow::show);

    return a.exec();
}