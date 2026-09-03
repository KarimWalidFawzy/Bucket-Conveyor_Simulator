#pragma once
#include <QImage>
#include <QColor>
#include <QString>
#include <QVector>
#include <QUuid>
#include <QMetaType>

struct DefectPatch {
    bool exists = false;
    double angularCoverageRad = 0.0; // 90° or 180°
    double initialPitchRad = 0.0;     // Initial angle relative to roll axis
};

struct SimulatedBall {
    QString trueId;
    QColor baseColor;
    double radius = 32.0;
    DefectPatch defect;
    int rollTicksElapsed = 0;

    SimulatedBall() : trueId(QUuid::createUuid().toString()) {}
};

struct BucketSlot {
    bool hasBall = false;
    SimulatedBall ball;
};

// CV observation for a single bucket slot in one frame
struct BucketObservation {
    int slotIndex = 0;       // 0 to 7
    uint64_t triggerIndex = 0;
    bool detectedBall = false;
    QColor measuredColor;
    double measuredRadius = 0.0;
    double defectAreaPixels = 0.0;
    bool hasDefectSignal = false;
};

// Temporal accumulation of a single ball across its 8-bucket lifespan
struct BallTrack {
    QString trackId;
    int currentSlotIndex = -1;
    int totalTriggersObserved = 0;
    
    QVector<BucketObservation> history;

    BallTrack() : trackId(QUuid::createUuid().toString()) {}
};

// Final output payload
struct BallFusedVerdict {
    QString trackId;
    QColor dominantColor;
    double avgRadius;
    bool isDefective;
    double maxDefectAreaObserved;
    int totalFramesTracked;
    double confidence = 0.0;
    QVector<uint64_t> contributingTriggers;
};

Q_DECLARE_METATYPE(BallFusedVerdict)