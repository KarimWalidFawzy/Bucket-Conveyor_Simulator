#pragma once
#include <QObject>
#include <QImage>
#include <QVector>
#include <QMap>
#include <QtMath>
#include <QDebug>
#include <optional>
#include <QUuid>
#include "ConveyorTypes.h"

class StationWorker : public QObject {
    Q_OBJECT
public:
    explicit StationWorker(int stationId, QObject *parent = nullptr)
        : QObject(parent), m_stationId(stationId) 
    {
        m_activeTracks.resize(8); // Slots 0..7
    }

public slots:
    void processFrame(int stationId, uint64_t triggerIndex, QImage frame) {
        if (stationId != m_stationId) return;

        QVector<BucketObservation> currentObservations(8);
        const double slotWidth = frame.width() / 8.0;

        for (int i = 0; i < 8; ++i) {
            const QRect roi(i * slotWidth, 0, slotWidth, frame.height());
            currentObservations[i] = extractFeaturesFromROI(i, frame.copy(roi));
        }

        if (m_activeTracks[7].has_value()) {
            finalizeTrackVerdict(m_activeTracks[7].value());
        }

        QVector<std::optional<BallTrack>> nextActiveTracks(8);

        for (int i = 0; i < 8; ++i) {
            const auto& obs = currentObservations[i];
            const std::optional<BallTrack> inherited = (i > 0) ? m_activeTracks[i - 1] : std::nullopt;

            if (obs.detectedBall) {
                if (inherited.has_value()) {
                    BallTrack track = inherited.value();
                    track.currentSlotIndex = i;
                    track.totalTriggersObserved += 1;
                    track.history.append(obs);
                    nextActiveTracks[i] = track;
                } else {
                    BallTrack newTrack;
                    newTrack.currentSlotIndex = i;
                    newTrack.totalTriggersObserved = 1;
                    newTrack.history.append(obs);
                    nextActiveTracks[i] = newTrack;
                }
            } else if (i == 7 && inherited.has_value()) {
                finalizeTrackVerdict(inherited.value());
            }
        }

        m_activeTracks = nextActiveTracks;
        emit processingCompleted(m_stationId, triggerIndex);
    }

signals:
    void ballVerdictFinalized(int stationId, BallFusedVerdict verdict);
    void processingCompleted(int stationId, uint64_t triggerIndex);

private:
    BucketObservation extractFeaturesFromROI(int slotIndex, const QImage& roiImg) {
        BucketObservation obs;
        obs.slotIndex = slotIndex;

        int w = roiImg.width();
        int h = roiImg.height();

        int ballPixelCount = 0;
        int defectPixelCount = 0;
        uint64_t sumR = 0, sumG = 0, sumB = 0;

        for (int y = 0; y < h; ++y) {
            const QRgb* line = reinterpret_cast<const QRgb*>(roiImg.scanLine(y));
            for (int x = 0; x < w; ++x) {
                QRgb p = line[x];
                int r = qRed(p), g = qGreen(p), b = qBlue(p);

                // Segment ball from conveyor background (> 45 brightness threshold)
                if (r > 45 || g > 45 || b > 45) {
                    ballPixelCount++;
                    sumR += r; sumG += g; sumB += b;

                    // Black defect mark segmentation (< 30 brightness)
                    if (r < 30 && g < 30 && b < 30) {
                        defectPixelCount++;
                    }
                }
            }
        }

        if (ballPixelCount > 600) { // Minimum area threshold
            obs.detectedBall = true;
            obs.measuredRadius = qSqrt(ballPixelCount / M_PI);
            obs.measuredColor = QColor(sumR / ballPixelCount, sumG / ballPixelCount, sumB / ballPixelCount);
            obs.defectAreaPixels = defectPixelCount;
            obs.hasDefectSignal = (defectPixelCount > 35);
        }

        return obs;
    }

    void finalizeTrackVerdict(const BallTrack& track) {
        if (track.history.isEmpty()) return;

        BallFusedVerdict verdict;
        verdict.trackId = track.trackId;
        verdict.totalFramesTracked = track.history.size();
        verdict.isDefective = false;
        verdict.maxDefectAreaObserved = 0.0;

        uint64_t rAcc = 0, gAcc = 0, bAcc = 0;
        double radAcc = 0.0;

        for (const auto& obs : track.history) {
            rAcc += obs.measuredColor.red();
            gAcc += obs.measuredColor.green();
            bAcc += obs.measuredColor.blue();
            radAcc += obs.measuredRadius;

            if (obs.hasDefectSignal) {
                verdict.isDefective = true;
            }
            if (obs.defectAreaPixels > verdict.maxDefectAreaObserved) {
                verdict.maxDefectAreaObserved = obs.defectAreaPixels;
            }
        }

        int count = track.history.size();
        verdict.dominantColor = QColor(rAcc / count, gAcc / count, bAcc / count);
        verdict.avgRadius = radAcc / count;

        emit ballVerdictFinalized(m_stationId, verdict);
    }

    int m_stationId;
    QVector<std::optional<BallTrack>> m_activeTracks;
};