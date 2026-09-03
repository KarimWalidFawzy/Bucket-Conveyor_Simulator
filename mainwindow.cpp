#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_manager = new ConveyorManager(3, this);
    connect(m_manager, &ConveyorManager::frameGenerated, this, &MainWindow::onFrameGenerated);
    connect(m_manager, &ConveyorManager::verdictReady, this, &MainWindow::onVerdictReady);
    m_manager->startPipelines();
    ui->statusbar->showMessage("3 stations running: 8-slot trigger pipeline ready");
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
    m_manager->triggerAll();
}

void MainWindow::onFrameGenerated(int stationId, uint64_t triggerIndex, QImage frame)
{
    if (stationId == 0) {
        ui->label->setPixmap(QPixmap::fromImage(frame).scaled(ui->label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->statusbar->showMessage(QString("Station 0 | trigger %1 | all 8 buckets captured").arg(triggerIndex));
    }
}

void MainWindow::onVerdictReady(int stationId, BallFusedVerdict verdict)
{
    ui->statusbar->showMessage(QString("Station %1 | ball %2 | %3 | %4 frames | confidence %5%%")
        .arg(stationId).arg(verdict.trackId.left(8))
        .arg(verdict.isDefective ? "REJECT" : "PASS")
        .arg(verdict.totalFramesTracked)
        .arg(qRound(verdict.confidence * 100.0)));
}

