#ifndef CONVEYORTYPES_H
#define CONVEYORTYPES_H
#include <QImage>
#include <QColor>
#include <QString>
#include <QVector>
#include <QUuid>
#include <memory>

// Surface defect patch specifications
struct DefectPatch {
    bool exists = false;
    double angularCoverageRad = 0.0; // 90 deg (PI/2) or 180 deg (PI)
    double centerAngleRad = 0.0;     // Initial position on sphere relative to roll axis
};

// Represents a physical ball on the line
struct Ball {
    QString id;
    QColor baseColor;
    double radius;
    DefectPatch defect;
    int rollTicksElapsed = 0;

    Ball() : id(QUuid::createUuid().toString()), radius(35.0) {}
};

// Represents one slot in the 8-bucket conveyor
struct BucketSlot {
    bool hasBall = false;
    Ball ball;
};

// CV analysis result per bucket in a single trigger frame
struct BucketObservation {
    int slotIndex;         // 0 to 7
    bool detectedBall;
    QColor detectedColor;
    double measuredRadius;
    double defectAreaPixels;
    bool hasDefect;
};

// Final fused verdict for a ball across its 8-bucket lifespan
struct BallFusedVerdict {
    QString trackId;
    QColor dominantColor;
    double avgRadius = 0.0;
    bool defectDetected = false;
    bool isDefective = false;
    double maxDefectAreaObserved = 0.0;
    int totalFramesTracked = 0;
};
#endif // CONVEYORTYPES_H
