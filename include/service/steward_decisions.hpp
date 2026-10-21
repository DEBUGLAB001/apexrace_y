#pragma once

#include <vector>
#include <string>
#include <utility>
#include "model/race_models.hpp"
#include "service/timing_engine.hpp"

namespace apex {

class StewardDecisions {
public:
    StewardDecisions();

    void initializeDefaultDecisions(TimingEngine& timingEngine);
    void reset();

    std::pair<bool, std::string> applyDecision(StewardDecision decision, TimingEngine& timingEngine);
    std::pair<bool, StewardDecision> undoLatestDecision(TimingEngine& timingEngine);

    const std::vector<StewardDecision>& getDecisionHistory() const { return decisionHistory_; }
    size_t getUndoStackDepth() const { return undoStack_.size(); }

private:
    std::vector<StewardDecision> decisionHistory_;
    std::vector<StewardDecision> undoStack_;
    int decisionCounter_ = 1;
};

} // namespace apex
