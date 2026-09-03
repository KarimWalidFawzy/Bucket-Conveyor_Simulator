#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "conveyormanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();
    void onFrameGenerated(int stationId, uint64_t triggerIndex, QImage frame);
    void onVerdictReady(int stationId, BallFusedVerdict verdict);

private:
    Ui::MainWindow *ui;
    ConveyorManager *m_manager;
};
#endif // MAINWINDOW_H
