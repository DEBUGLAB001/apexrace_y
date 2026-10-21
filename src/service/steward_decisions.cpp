#include "service/steward_decisions.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace apex {

StewardDecisions::StewardDecisions() {
    decisionCounter_ = 1;
}

void StewardDecisions::initializeDefaultDecisions(TimingEngine& timingEngine) {
    decisionHistory_.clear();
    undoStack_.clear();
    decisionCounter_ = 1;

    // Apply baseline warning to Car #11
    StewardDecision d1;
    d1.decisionId = "dec_001";
    d1.incidentId = "inc_000";
    d1.carNumber = 11;
    d1.driverId = "drv_per";
    d1.actionType = "WARNING";
    d1.penaltySeconds = 0;
    d1.reason = "Track limits violation at Turn 14 exit.";
    d1.timestamp = "14:11:05";
    d1.reversible = true;

    applyDecision(d1, timingEngine);
}

void StewardDecisions::reset() {
    decisionHistory_.clear();
    undoStack_.clear();
    decisionCounter_ = 1;
}

std::pair<bool, std::string> StewardDecisions::applyDecision(StewardDecision decision,
                                                             TimingEngine& timingEngine) {
    Driver* d = nullptr;
    if (!decision.driverId.empty()) {
        d = timingEngine.getDriver(decision.driverId);
    }
    if (!d && decision.carNumber > 0) {
        d = timingEngine.getDriverByNumber(decision.carNumber);
        if (d) decision.driverId = d->id;
    }

    if (!d) {
        return {false, "Driver not found for decision"};
    }

    if (decision.decisionId.empty()) {
        std::ostringstream ss;
        ss << "dec_" << std::setw(3) << std::setfill('0') << decisionCounter_++;
        decision.decisionId = ss.str();
    }

    // Capture snapshot of driver state prior to this action
    decision.prevTotalPenaltySeconds = d->totalPenaltySeconds;
    decision.prevWarningsCount = d->warningsCount;
    decision.prevIsDisqualified = d->isDisqualified;

    // Apply the configured steward penalty
    if (decision.actionType == "TIME_PENALTY") {
        d->totalPenaltySeconds += decision.penaltySeconds;
    } else if (decision.actionType == "WARNING") {
        d->warningsCount += 1;
    } else if (decision.actionType == "DISQUALIFICATION") {
        d->isDisqualified = true;
    }

    timingEngine.recalculateLeaderboard();

    decisionHistory_.push_back(decision);
    if (decision.reversible) {
        undoStack_.push_back(decision);
    }

    std::ostringstream logMsg;
    logMsg << "Steward Decision [" << decision.decisionId << "]: "
           << decision.actionType << " applied to Car #" << decision.carNumber
           << " (" << decision.reason << ")";
    timingEngine.addSessionLog(logMsg.str());

    return {true, decision.decisionId};
}

std::pair<bool, StewardDecision> StewardDecisions::undoLatestDecision(TimingEngine& timingEngine) {
    if (undoStack_.empty()) {
        return {false, StewardDecision{}};
    }

    StewardDecision top = undoStack_.back();
    undoStack_.pop_back();

    // Remove from visible decision history
    for (auto it = decisionHistory_.begin(); it != decisionHistory_.end(); ++it) {
        if (it->decisionId == top.decisionId) {
            decisionHistory_.erase(it);
            break;
        }
    }

    // Restore affected driver state
    Driver* d = timingEngine.getDriver(top.driverId);
    if (!d && top.carNumber > 0) {
        d = timingEngine.getDriverByNumber(top.carNumber);
    }

    if (d) {
#ifdef SEED_BUG_6
        // Bug 6 — Stack-Based Undo defect:
        // Defect: Fails to restore totalPenaltySeconds (only restores warningsCount),
        // leaving time penalty seconds applied to the driver on the timing tower!
        d->warningsCount = top.prevWarningsCount;
        // Missing: d->totalPenaltySeconds = top.prevTotalPenaltySeconds;
#else
        // Correct behavior: restores all previous driver state fields completely
        d->totalPenaltySeconds = top.prevTotalPenaltySeconds;
        d->warningsCount = top.prevWarningsCount;
        d->isDisqualified = top.prevIsDisqualified;
#endif
        timingEngine.recalculateLeaderboard();

        std::ostringstream logMsg;
        logMsg << "Steward Decision [" << top.decisionId << "] UNDONE for Car #"
               << top.carNumber;
        timingEngine.addSessionLog(logMsg.str());
    }

    return {true, top};
}

} // namespace apex
