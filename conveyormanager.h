#pragma once
#include <QObject>
#include <QThread>
#include <QVector>
#include <QDebug>
#include <QElapsedTimer>
#include "ConveyorStation.h"
#include "StationWorker.h"

class ConveyorManager : public QObject {
    Q_OBJECT
public:
    explicit ConveyorManager(int stationCount = 3, QObject *parent = nullptr)
        : QObject(parent)
    {
        qRegisterMetaType<BallFusedVerdict>("BallFusedVerdict");
        for (int i = 0; i < stationCount; ++i) {
            auto* station = new ConveyorStation(i);
            auto* worker = new StationWorker(i);
            auto* thread = new QThread(this);

            station->moveToThread(thread);
            worker->moveToThread(thread);

            connect(station, &ConveyorStation::frameGenerated, worker, &StationWorker::processFrame);
            connect(station, &ConveyorStation::frameGenerated, this, &ConveyorManager::frameGenerated);
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

    void triggerAll() {
        for (ConveyorStation* station : m_stations) {
            QMetaObject::invokeMethod(station, "triggerOnce", Qt::QueuedConnection);
        }
    }

signals:
    void frameGenerated(int stationId, uint64_t triggerIndex, QImage frame);
    void verdictReady(int stationId, BallFusedVerdict verdict);

private slots:
    void onVerdictReceived(int stationId, BallFusedVerdict verdict) {
        QStringList frames;
        for (uint64_t trigger : verdict.contributingTriggers) frames.append(QString::number(trigger));
        qInfo().noquote() << QString("[STATION %1 VERDICT] Track: %2 | Frames: %3 (%4/8) | Color: RGB(%5,%6,%7) | Result: %8")
            .arg(stationId)
            .arg(verdict.trackId.left(8))
            .arg(frames.join(','))
            .arg(verdict.totalFramesTracked)
            .arg(verdict.dominantColor.red())
            .arg(verdict.dominantColor.green())
            .arg(verdict.dominantColor.blue())
            .arg(verdict.isDefective ? "REJECT (Defect Detected)" : "PASS (Clean)");
        emit verdictReady(stationId, verdict);
    }

private:
    QVector<ConveyorStation*> m_stations;
    QVector<StationWorker*> m_workers;
    QVector<QThread*> m_threads;
};
