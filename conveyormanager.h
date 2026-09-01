#pragma once
#include <QObject>
#include <QThread>
#include <QVector>
#include <QDebug>
#include "ConveyorStation.h"
#include "StationWorker.h"

class ConveyorManager : public QObject {
    Q_OBJECT
public:
    explicit ConveyorManager(int stationCount = 3, QObject *parent = nullptr)
        : QObject(parent)
    {
        for (int i = 0; i < stationCount; ++i) {
            auto* station = new ConveyorStation(i);
            auto* worker = new StationWorker(i);
            auto* thread = new QThread(this);

            station->moveToThread(thread);
            worker->moveToThread(thread);

            connect(station, &ConveyorStation::frameGenerated, worker, &StationWorker::processFrame);
            connect(worker, &StationWorker::ballVerdictFinalized, this, &ConveyorManager::onVerdictReceived);

            connect(thread, &QThread::finished, worker, &QObject::deleteLater);
            connect(thread, &QThread::finished, station, &QObject::deleteLater);

            thread->start();

            m_stations.append(station);
            m_workers.append(worker);
            m_threads.append(thread);
        }
    }

    ~ConveyorManager() {
        for (auto* t : m_threads) {
            t->quit();
            t->wait();
        }
    }

    void startPipelines(int baseSpeedMs = 120) {
        for (int i = 0; i < m_stations.size(); ++i) {
            // Introduce phase/speed variances across camera stations
            int stationSpeed = baseSpeedMs + (i * 25);
            QMetaObject::invokeMethod(m_stations[i], "start", Qt::QueuedConnection, Q_ARG(int, stationSpeed));
        }
    }

private slots:
    void onVerdictReceived(int stationId, BallFusedVerdict verdict) {
        qInfo().noquote() << QString("[STATION %1 VERDICT] Track: %2 | Obs Count: %3/8 | Color: RGB(%4,%5,%6) | Result: %7")
            .arg(stationId)
            .arg(verdict.trackId.left(8))
            .arg(verdict.totalFramesTracked)
            .arg(verdict.dominantColor.red())
            .arg(verdict.dominantColor.green())
            .arg(verdict.dominantColor.blue())
            .arg(verdict.isDefective ? "REJECT (Defect Detected)" : "PASS (Clean)");
    }

private:
    QVector<ConveyorStation*> m_stations;
    QVector<StationWorker*> m_workers;
    QVector<QThread*> m_threads;
};
