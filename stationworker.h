#pragma once
#include <QObject>
#include <QImage>
#include <QMap>
#include <QDebug>
#include "ConveyorTypes.h"

class StationWorker : public QObject {
    Q_OBJECT
public:
    explicit StationWorker(int stationId, QObject *parent = nullptr)
        : QObject(parent), m_stationId(stationId) {}

public slots:
    void processFrame(int stationId, uint64_t triggerIndex, QImage frame, QVector<BucketSlot> groundTruthState) {
        if (stationId != m_stationId) return;

        QVector<BucketObservation> observations;
        observations.reserve(8);
        const double slotWidth = frame.width() / 8.0;

        // Process 8 bucket ROIs independently.
        for (int i = 0; i < 8; ++i) {
            QRect roi(i * slotWidth, 0, slotWidth, frame.height());
            QImage bucketImg = frame.copy(roi);
            observations.append(analyzeBucketROI(i, bucketImg));
        }

        // Finalize any ball that just left the visible belt before the internal slot map shifts.
        shiftTracks();

        // Rebuild the slot-to-track mapping from the physical ground truth state.
        for (int i = 0; i < 8; ++i) {
            const auto& gtSlot = groundTruthState[i];
            if (gtSlot.hasBall) {
                QString ballId = gtSlot.ball.id;
                m_activeSlotBallIds[i] = ballId;

                if (observations[i].detectedBall) {
                    auto& history = m_ballHistories[ballId];
                    history.append(observations[i]);
                }
            } else {
                m_activeSlotBallIds[i] = "";
            }
        }

        emit processingCompleted(m_stationId, triggerIndex, observations);
    }

signals:
    void ballVerdictFinalized(int stationId, BallFusedVerdict verdict);
    void processingCompleted(int stationId, uint64_t triggerIndex, QVector<BucketObservation> obs);

private:
    void shiftTracks() {
        // If a ball was in bucket 7 (last slot), it exits line -> finalize its verdict
        if (!m_activeSlotBallIds[7].isEmpty()) {
            QString exitingId = m_activeSlotBallIds[7];
            if (m_ballHistories.contains(exitingId)) {
                finalizeVerdict(exitingId);
            }
        }

        // Shift internal tracking array to match conveyor movement
        for (int i = 7; i > 0; --i) {
            m_activeSlotBallIds[i] = m_activeSlotBallIds[i - 1];
        }
        m_activeSlotBallIds[0] = "";
    }

    BucketObservation analyzeBucketROI(int slotIdx, const QImage& img) {
        BucketObservation obs;
        obs.slotIndex = slotIdx;
        obs.detectedBall = false;
        obs.hasDefect = false;
        obs.defectAreaPixels = 0.0;

        int w = img.width();
        int h = img.height();

        // Classical Image Processing: Color Thresholding & Foreground Segmentation
        int nonBgPixels = 0;
        int blackDefectPixels = 0;
        int rSum = 0, gSum = 0, bSum = 0;

        for (int y = 0; y < h; ++y) {
            const QRgb* scanline = reinterpret_cast<const QRgb*>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                QRgb pixel = scanline[x];
                int r = qRed(pixel);
                int g = qGreen(pixel);
                int b = qBlue(pixel);

                // Background thresholding (belt color ~40,40,40)
                if (r > 50 || g > 50 || b > 50) {
                    nonBgPixels++;
                    rSum += r; gSum += g; bSum += b;

                    // Black defect patch detection on surface (low intensity)
                    if (r < 25 && g < 25 && b < 25) {
                        blackDefectPixels++;
                    }
                }
            }
        }

        // Min bounding area threshold to confirm presence of ball
        if (nonBgPixels > 800) { // Approx circle radius > 16px
            obs.detectedBall = true;
            obs.measuredRadius = qSqrt(nonBgPixels / M_PI);
            obs.detectedColor = QColor(rSum / nonBgPixels, gSum / nonBgPixels, bSum / nonBgPixels);
            obs.defectAreaPixels = blackDefectPixels;

            // Defect present if defect pixel count exceeds spatial noise floor
            if (blackDefectPixels > 45) {
                obs.hasDefect = true;
            }
        }

        return obs;
    }

    void finalizeVerdict(const QString& ballId) {
        const auto& history = m_ballHistories[ballId];
        BallFusedVerdict verdict;
        verdict.trackId = ballId;
        verdict.totalFramesTracked = history.size();
        verdict.defectDetected = false;
        verdict.isDefective = false;
        verdict.maxDefectAreaObserved = 0.0;

        double totalR = 0, totalG = 0, totalB = 0;
        double radAccum = 0;

        for (const auto& obs : history) {
            totalR += obs.detectedColor.red();
            totalG += obs.detectedColor.green();
            totalB += obs.detectedColor.blue();
            radAccum += obs.measuredRadius;

            if (obs.hasDefect) {
                verdict.defectDetected = true;
                verdict.isDefective = true;
            }
            if (obs.defectAreaPixels > verdict.maxDefectAreaObserved) {
                verdict.maxDefectAreaObserved = obs.defectAreaPixels;
            }
        }

        if (!history.isEmpty()) {
            verdict.dominantColor = QColor(totalR / history.size(), totalG / history.size(), totalB / history.size());
            verdict.avgRadius = radAccum / history.size();
        }

        emit ballVerdictFinalized(m_stationId, verdict);
        m_ballHistories.remove(ballId);
    }

    int m_stationId;
    QVector<QString> m_activeSlotBallIds = QVector<QString>(8, "");
    QMap<QString, QVector<BucketObservation>> m_ballHistories;
};
