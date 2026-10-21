#pragma once

#include <string>
#include <vector>
#include "json.hpp"

namespace apex {

struct LapRecord {
    int lapNumber = 0;
    double lapTime = 0.0;
    double s1 = 0.0;
    double s2 = 0.0;
    double s3 = 0.0;
    bool isPersonalBest = false;
    bool isOverallBest = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LapRecord,
    lapNumber, lapTime, s1, s2, s3, isPersonalBest, isOverallBest)

struct Driver {
    std::string id;
    int carNumber = 0;
    std::string name;
    std::string team;
    int position = 0;
    int previousPosition = 0;
    int completedLaps = 0;
    double lastLapTime = 0.0;
    double bestLapTime = 0.0;
    double s1Time = 0.0;
    double s2Time = 0.0;
    double s3Time = 0.0;
    double gapToLeader = 0.0;
    double intervalToAhead = 0.0;
    std::string pitStatus = "ON TRACK";
    int totalPenaltySeconds = 0;
    int warningsCount = 0;
    bool isDisqualified = false;
    bool isFastestLap = false;
    std::string tireCompound = "MEDIUM";
    int tireAgeLaps = 0;
    std::vector<LapRecord> lapHistory;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Driver,
    id, carNumber, name, team, position, previousPosition, completedLaps,
    lastLapTime, bestLapTime, s1Time, s2Time, s3Time,
    gapToLeader, intervalToAhead, pitStatus, totalPenaltySeconds,
    warningsCount, isDisqualified, isFastestLap, tireCompound, tireAgeLaps,
    lapHistory)

struct RadioMessage {
    std::string id;
    std::string sender;
    int carNumber = 0;
    std::string timestamp;
    std::string category = "GENERAL"; // STRATEGY, INCIDENT, TECHNICAL, STEWARDS, GENERAL
    std::string text;
    bool isFlagged = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RadioMessage,
    id, sender, carNumber, timestamp, category, text, isFlagged)

struct PitStop {
    std::string id;
    int carNumber = 0;
    int plannedLap = 0;
    std::string stopType = "MEDIUM"; // SOFT, MEDIUM, HARD, INTERMEDIATE, WET, WING_CHANGE, PENALTY_SERVE
    std::string status = "SCHEDULED"; // SCHEDULED, CONFIRMED, COMPLETED, CANCELLED
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PitStop,
    id, carNumber, plannedLap, stopType, status)

struct Incident {
    std::string id;
    int lap = 0;
    std::string location;
    std::vector<int> involvedCars;
    std::string category = "COLLISION"; // COLLISION, TRACK_LIMITS, SPEEDING_PIT_LANE, IMPEDING, UNSAFE_RELEASE, DEBRIS
    std::string severity = "MEDIUM"; // LOW, MEDIUM, HIGH, CRITICAL
    std::string description;
    int arrivalSequence = 0;
    std::string status = "PENDING"; // PENDING, UNDER_INVESTIGATION, RESOLVED, NO_ACTION
    std::string reviewNotes;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Incident,
    id, lap, location, involvedCars, category, severity, description,
    arrivalSequence, status, reviewNotes)

struct StewardDecision {
    std::string decisionId;
    std::string incidentId;
    int carNumber = 0;
    std::string driverId;
    std::string actionType = "WARNING"; // WARNING, TIME_PENALTY, STOP_GO, DISQUALIFICATION, NO_FURTHER_ACTION
    int penaltySeconds = 0;
    std::string reason;
    std::string timestamp;
    bool reversible = true;

    // Snapshot of driver state before this action was applied
    int prevTotalPenaltySeconds = 0;
    int prevWarningsCount = 0;
    bool prevIsDisqualified = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StewardDecision,
    decisionId, incidentId, carNumber, driverId, actionType, penaltySeconds,
    reason, timestamp, reversible, prevTotalPenaltySeconds, prevWarningsCount,
    prevIsDisqualified)

struct SessionState {
    std::string eventName = "Apex British Grand Prix";
    std::string circuit = "Silverstone Circuit";
    double circuitLengthKm = 5.891;
    std::string sessionType = "Race";
    int sessionClockSeconds = 1425;
    int currentLap = 16;
    int totalLaps = 52;
    std::string trackStatus = "GREEN"; // GREEN, YELLOW, SAFETY_CAR, RED_FLAG
    bool isSimRunning = false;
    std::vector<std::string> eventLogs;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SessionState,
    eventName, circuit, circuitLengthKm, sessionType, sessionClockSeconds,
    currentLap, totalLaps, trackStatus, isSimRunning, eventLogs)

} // namespace apex
