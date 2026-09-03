/*#include <QCoreApplication>
#include <QTimer>
#include "conveyormanager.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "Starting Multi-Camera Conveyor Tracking System...";

    ConveyorManager manager(3); // Run 3 parallel camera pipelines
    manager.startPipelines(100);

    QTimer::singleShot(10000, &app, &QCoreApplication::quit);
    return app.exec();
}
*/
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
