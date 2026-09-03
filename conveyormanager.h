#pragma once
#include <QObject>
#include <QThread>
#include <QVector>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QTextStream>
#include "ConveyorStation.h"
#include "StationWorker.h"

class ConveyorManager : public QObject {
    Q_OBJECT
public:
    explicit ConveyorManager(int stationCount = 3, QObject *parent = nullptr)
        : QObject(parent)
        , m_resultsFile("results.csv")
        , m_resultsStream(&m_resultsFile)
        , m_metricsFile("pipeline_metrics.csv")
        , m_metricsStream(&m_metricsFile)
    {
        qRegisterMetaType<BallFusedVerdict>("BallFusedVerdict");
        m_metricsClock.start();
        if (m_resultsFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            m_resultsStream << "station_id,track_id,result,frames_tracked,confidence,dominant_red,dominant_green,dominant_blue,diameter_mm,max_defect_area,contributing_triggers\n";
            m_resultsStream.flush();
        } else {
            qWarning() << "Could not open results.csv for writing:" << m_resultsFile.errorString();
        }
        if (m_metricsFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            m_metricsStream << "station_id,trigger_index,latency_ms,pending_frames,generated_frames,processed_frames,effective_throughput_fps\n";
            m_metricsStream.flush();
        } else {
            qWarning() << "Could not open pipeline_metrics.csv for writing:" << m_metricsFile.errorString();
        }

        for (int i = 0; i < stationCount; ++i) {
            auto* station = new ConveyorStation(i);
            auto* worker = new StationWorker(i);
            auto* thread = new QThread(this);

            station->moveToThread(thread);
            worker->moveToThread(thread);

            connect(station, &ConveyorStation::frameGenerated, worker, &StationWorker::processFrame);
            connect(station, &ConveyorStation::frameGenerated, this, &ConveyorManager::frameGenerated);
            connect(station, &ConveyorStation::frameGenerated, this, &ConveyorManager::onFrameObserved);
            connect(worker, &StationWorker::ballVerdictFinalized, this, &ConveyorManager::onVerdictReceived);
            connect(worker, &StationWorker::processingCompleted, this, &ConveyorManager::onProcessingCompleted);

            connect(thread, &QThread::finished, worker, &QObject::deleteLater);
            connect(thread, &QThread::finished, station, &QObject::deleteLater);

            thread->start();

            m_stations.append(station);
            m_workers.append(worker);
            m_threads.append(thread);
        }
    }

    ~ConveyorManager() {
        for (int i = 0; i < m_threads.size(); ++i) {
            QMetaObject::invokeMethod(m_stations[i], "stop", Qt::BlockingQueuedConnection);
            QMetaObject::invokeMethod(m_workers[i], "flushActiveTracks", Qt::BlockingQueuedConnection);
        }
        for (auto* t : m_threads) {
            t->quit();
            t->wait();
        }
        m_resultsStream.flush();
        m_resultsFile.close();
        m_metricsStream.flush();
        m_metricsFile.close();
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
    void onFrameObserved(int stationId, uint64_t triggerIndex, QImage frame) {
        Q_UNUSED(frame);
        m_generatedFrames[stationId]++;
        m_pendingFrames[stationId]++;
        m_maxPendingFrames[stationId] = qMax(m_maxPendingFrames.value(stationId), m_pendingFrames.value(stationId));
        m_frameStartTimes[stationId].insert(triggerIndex, m_metricsClock.elapsed());
    }

    void onProcessingCompleted(int stationId, uint64_t triggerIndex) {
        const qint64 now = m_metricsClock.elapsed();
        const qint64 start = m_frameStartTimes[stationId].take(triggerIndex);
        const qint64 latency = qMax<qint64>(0, now - start);
        m_processedFrames[stationId]++;
        m_pendingFrames[stationId] = qMax(0, m_generatedFrames.value(stationId) - m_processedFrames.value(stationId));

        const double elapsedSeconds = qMax(0.001, now / 1000.0);
        const double throughput = m_processedFrames.value(stationId) / elapsedSeconds;
        if (m_metricsFile.isOpen()) {
            m_metricsStream << stationId << ',' << triggerIndex << ',' << latency << ','
                            << m_pendingFrames.value(stationId) << ','
                            << m_generatedFrames.value(stationId) << ','
                            << m_processedFrames.value(stationId) << ','
                            << throughput << '\n';
            m_metricsStream.flush();
        }

        if (m_processedFrames.value(stationId) % 100 == 0) {
            qInfo().noquote() << QString("[STATION %1 METRICS] %2 frames | %3 fps | latency %4 ms | pending %5 | max pending %6")
                .arg(stationId)
                .arg(m_processedFrames.value(stationId))
                .arg(throughput, 0, 'f', 2)
                .arg(latency)
                .arg(m_pendingFrames.value(stationId))
                .arg(m_maxPendingFrames.value(stationId));
        }
    }

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

        if (m_resultsFile.isOpen()) {
            m_resultsStream << stationId << ','
                            << csvField(verdict.trackId) << ','
                            << (verdict.isDefective ? "REJECT" : "PASS") << ','
                            << verdict.totalFramesTracked << ','
                            << verdict.confidence << ','
                            << verdict.dominantColor.red() << ','
                            << verdict.dominantColor.green() << ','
                            << verdict.dominantColor.blue() << ','
                            << verdict.avgDiameterMm << ','
                            << verdict.maxDefectAreaObserved << ','
                            << csvField(frames.join(';')) << '\n';
            m_resultsStream.flush();
        }

        emit verdictReady(stationId, verdict);
    }

private:
    static QString csvField(const QString& value) {
        QString escaped = value;
        escaped.replace('"', "\"\"");
        return '"' + escaped + '"';
    }

    QVector<ConveyorStation*> m_stations;
    QVector<StationWorker*> m_workers;
    QVector<QThread*> m_threads;
    QFile m_resultsFile;
    QTextStream m_resultsStream;
    QFile m_metricsFile;
    QTextStream m_metricsStream;
    QElapsedTimer m_metricsClock;
    QHash<int, QHash<uint64_t, qint64>> m_frameStartTimes;
    QHash<int, int> m_generatedFrames;
    QHash<int, int> m_processedFrames;
    QHash<int, int> m_pendingFrames;
    QHash<int, int> m_maxPendingFrames;
};
