#pragma once
#include <QObject>
#include <QTimer>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QtMath>
#include "ConveyorTypes.h"

class ConveyorStation : public QObject {
    Q_OBJECT
public:
    explicit ConveyorStation(int stationId, QObject *parent = nullptr)
        : QObject(parent), m_stationId(stationId) 
    {
        m_buckets.resize(8);
    }

public slots:
    void start(int intervalMs) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &ConveyorStation::onTrigger);
        m_timer->start(intervalMs);
    }

    void stop() {
        if (m_timer) m_timer->stop();
    }

signals:
    void frameGenerated(int stationId, uint64_t triggerIndex, QImage frame);

private slots:
    void onTrigger() {
        m_triggerCount++;

        // Every ball already in the line advances its rolling state once per trigger.
        for (int i = 0; i < 8; ++i) {
            if (m_buckets[i].hasBall) {
                m_buckets[i].ball.rollTicksElapsed++;
            }
        }

        // Shift the 8-slot conveyor one step to the right. Empty slots move as well.
        QVector<BucketSlot> shifted(8);
        for (int i = 7; i > 0; --i) {
            shifted[i] = m_buckets[i - 1];
        }

        // Slot 1 (index 0) is the entry point for a fresh item each trigger.
        shifted[0] = spawnNewBucketSlot();
        m_buckets = shifted;

        QImage frame = renderConveyorFrame();
        emit frameGenerated(m_stationId, m_triggerCount, frame);
    }

private:
    BucketSlot spawnNewBucketSlot() {
        BucketSlot slot;
        if (QRandomGenerator::global()->bounded(100) < 70) {
            slot.hasBall = true;
            slot.ball.rollTicksElapsed = 0;

            static const QVector<QColor> palette = {
                QColor(220, 40, 40),
                QColor(245, 130, 32),
                QColor(240, 200, 20),
                QColor(40, 160, 50)
            };
            slot.ball.baseColor = palette[QRandomGenerator::global()->bounded(palette.size())];

            if (QRandomGenerator::global()->bounded(100) < 40) {
                slot.ball.defect.exists = true;
                slot.ball.defect.angularCoverageRad = (QRandomGenerator::global()->bounded(2) == 0) ? M_PI_2 : M_PI;
                slot.ball.defect.initialPitchRad = QRandomGenerator::global()->bounded(360) * M_PI / 180.0;
            }
        }

        return slot;
    }

    QImage renderConveyorFrame() {
        QImage image(800, 120, QImage::Format_RGB32);
        image.fill(QColor(25, 25, 25));

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);

        const double slotWidth = 100.0;
        const double centerY = 60.0;

        painter.setPen(QPen(QColor(90, 90, 90), 2));
        for (int i = 1; i < 8; ++i) {
            painter.drawLine(QLineF(i * slotWidth, 0, i * slotWidth, 120));
        }

        for (int i = 0; i < 8; ++i) {
            if (!m_buckets[i].hasBall) {
                continue;
            }

            const SimulatedBall& ball = m_buckets[i].ball;
            const double centerX = (i + 0.5) * slotWidth;
            const double r = ball.radius;

            painter.setPen(Qt::NoPen);
            painter.setBrush(ball.baseColor);
            painter.drawEllipse(QPointF(centerX, centerY), r, r);

            if (ball.defect.exists) {
                const double currentPitch = ball.defect.initialPitchRad + (ball.rollTicksElapsed * (M_PI / 4.0));
                const double sinPitch = qSin(currentPitch);
                const double cosPitch = qCos(currentPitch);
                const double normalizedVisible = qBound(0.0, qAbs(sinPitch), 1.0);
                const double patchWidth = r * (0.35 + 0.65 * normalizedVisible);
                const double patchHeight = r * (0.20 + 0.80 * normalizedVisible) * (ball.defect.angularCoverageRad / M_PI);
                const double offsetY = -r * cosPitch * 0.35;

                if (normalizedVisible > 0.05) {
                    painter.save();
                    QPainterPath clipPath;
                    clipPath.addEllipse(QPointF(centerX, centerY), r, r);
                    painter.setClipPath(clipPath);
                    painter.setBrush(Qt::black);
                    painter.drawEllipse(QRectF(centerX - patchWidth / 2.0,
                                               centerY + offsetY - patchHeight / 2.0,
                                               patchWidth,
                                               patchHeight));
                    painter.restore();
                }
            }

            QRadialGradient grad(centerX - r * 0.3, centerY - r * 0.3, r);
            grad.setColorAt(0.0, QColor(255, 255, 255, 100));
            grad.setColorAt(0.8, QColor(0, 0, 0, 40));
            grad.setColorAt(1.0, QColor(0, 0, 0, 120));
            painter.setBrush(grad);
            painter.drawEllipse(QPointF(centerX, centerY), r, r);
        }

        return image;
    }

    int m_stationId;
    uint64_t m_triggerCount = 0;
    QTimer* m_timer = nullptr;
    QVector<BucketSlot> m_buckets;
};