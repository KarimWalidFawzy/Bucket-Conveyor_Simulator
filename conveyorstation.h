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
        for(auto& slot : m_buckets) slot.hasBall = false;
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
    void frameGenerated(int stationId, uint64_t triggerIndex, QImage frame, QVector<BucketSlot> stateSnapshot);

private slots:
    void onTrigger() {
        m_triggerCount++;

        // Shift conveyor right (slot 7 exits, slot 0 gets new input)
        for (int i = 7; i > 0; --i) {
            m_buckets[i] = m_buckets[i - 1];
        }

        // Advance roll state for all balls currently on the belt
        for (int i = 1; i < 8; ++i) {
            if (m_buckets[i].hasBall) {
                m_buckets[i].ball.rollTicksElapsed++;
            }
        }

        // Spawn new ball (~70% probability) or leave empty (~30%)
        BucketSlot newSlot;
        if (QRandomGenerator::global()->bounded(100) < 70) {
            newSlot.hasBall = true;
            newSlot.ball.rollTicksElapsed = 0;

            // Random Base Palette
            static const QVector<QColor> palette = { Qt::red, QColor(255,140,0), Qt::yellow, Qt::green, Qt::blue, QColor(148,0,211) };
            newSlot.ball.baseColor = palette[QRandomGenerator::global()->bounded(palette.size())];

            // 40% Defect Probability
            if (QRandomGenerator::global()->bounded(100) < 40) {
                newSlot.ball.defect.exists = true;
                newSlot.ball.defect.angularCoverageRad = (QRandomGenerator::global()->bounded(2) == 0) ? M_PI_2 : M_PI;
                newSlot.ball.defect.centerAngleRad = QRandomGenerator::global()->bounded(360) * M_PI / 180.0;
            }
        } else {
            newSlot.hasBall = false;
        }
        m_buckets[0] = newSlot;

        // Render synthetically generated trigger snapshot frame (800x150)
        QImage frame = renderFrame();
        emit frameGenerated(m_stationId, m_triggerCount, frame, m_buckets);
    }

private:
    QImage renderFrame() {
        QImage image(800, 150, QImage::Format_RGB32);
        image.fill(QColor(40, 40, 40)); // Dark belt background

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);

        const double slotWidth = 100.0;
        const double centerY = 75.0;

        // Draw bucket divider lines
        painter.setPen(QPen(Qt::gray, 2, Qt::DashLine));
        for (int i = 1; i < 8; ++i) {
            painter.drawLine(QLineF(i * slotWidth, 0, i * slotWidth, 150));
        }

        // Render each slot contents
        for (int i = 0; i < 8; ++i) {
            double centerX = (i + 0.5) * slotWidth;

            if (!m_buckets[i].hasBall) {
                // Render empty bucket marker
                painter.setPen(QPen(QColor(80, 80, 80), 1));
                painter.drawRect(QRectF(i * slotWidth + 10, 15, 80, 120));
                continue;
            }

            const Ball& ball = m_buckets[i].ball;
            double r = ball.radius;

            // Draw Base Ball Surface
            painter.setPen(Qt::NoPen);
            painter.setBrush(ball.baseColor);
            painter.drawEllipse(QPointF(centerX, centerY), r, r);

            // Compute 3D Rolling Defect Projection
            if (ball.defect.exists) {
                // Pitch rotation around horizontal axis: 45 deg per trigger tick
                double rollAngle = ball.defect.centerAngleRad + (ball.rollTicksElapsed * (M_PI / 4.0));
                rollAngle = fmod(rollAngle, 2.0 * M_PI);

                // Check if patch is currently facing top hemisphere visible to camera (-PI/2 to PI/2)
                double cosVal = qCos(rollAngle);
                double sinVal = qSin(rollAngle);

                if (sinVal > -0.2) { // Slightly past horizon for partial edge visibility
                    painter.save();
                    painter.setClipPath([&]() {
                        QPainterPath p;
                        p.addEllipse(QPointF(centerX, centerY), r, r);
                        return p;
                    }());

                    // Apparent height projected under X-axis rolling transform
                    double patchHeight = r * qAbs(sinVal) * (ball.defect.angularCoverageRad / M_PI);
                    double patchWidth = r * (ball.defect.angularCoverageRad / M_PI);
                    double offsetY = -r * cosVal * 0.5;

                    painter.setBrush(Qt::black);
                    painter.drawEllipse(QRectF(centerX - patchWidth / 2.0, centerY + offsetY - patchHeight / 2.0, patchWidth, patchHeight));
                    painter.restore();
                }
            }

            // Draw specular highlight to reflect 3D sphere shape
            QRadialGradient grad(centerX - r * 0.3, centerY - r * 0.3, r);
            grad.setColorAt(0.0, QColor(255, 255, 255, 120));
            grad.setColorAt(0.5, QColor(255, 255, 255, 0));
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
