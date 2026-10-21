#include "service/incident_queue.hpp"
#include <iomanip>
#include <sstream>

namespace apex {

IncidentQueue::IncidentQueue() {
    initializeDefaultIncidents();
}

void IncidentQueue::initializeDefaultIncidents() {
    pendingQueue_.clear();
    resolvedIncidents_.clear();
    sequenceCounter_ = 1;

    // Seed resolved incident
    Incident res;
    res.id = "inc_000";
    res.lap = 8;
    res.location = "Turn 14 (Chapel)";
    res.involvedCars = {11};
    res.category = "TRACK_LIMITS";
    res.severity = "LOW";
    res.description = "Car 11 exceeded track limits at exit.";
    res.arrivalSequence = 0;
    res.status = "RESOLVED";
    res.reviewNotes = "First track limits strike recorded. Warning issued.";
    resolvedIncidents_.push_back(res);

    // Seed 3 pending incidents in arrival order
    reportIncident(14, "Turn 4 (The Loop)", {55, 63}, "COLLISION", "MEDIUM",
                   "Wheel-to-wheel contact between SAI and RUS while contesting position.");
    reportIncident(15, "Turn 9 (Copse)", {1}, "TRACK_LIMITS", "LOW",
                   "Car 1 exceeded track limits on exit of Turn 9 (third recorded warning).");
    reportIncident(16, "Pit Exit", {23}, "UNSAFE_RELEASE", "HIGH",
                   "Car 23 released into path of incoming traffic in fast lane.");
}

void IncidentQueue::reset() {
    initializeDefaultIncidents();
}

Incident IncidentQueue::reportIncident(int lap,
                                     const std::string& location,
                                     const std::vector<int>& involvedCars,
                                     const std::string& category,
                                     const std::string& severity,
                                     const std::string& description) {
    Incident inc;
    std::ostringstream ss;
    ss << "inc_" << std::setw(3) << std::setfill('0') << sequenceCounter_;
    inc.id = ss.str();
    inc.lap = lap;
    inc.location = location;
    inc.involvedCars = involvedCars;
    inc.category = category;
    inc.severity = severity;
    inc.description = description;
    inc.arrivalSequence = sequenceCounter_++;
    inc.status = "PENDING";
    inc.reviewNotes = "";

    pendingQueue_.push_back(inc);
    return inc;
}

std::vector<Incident> IncidentQueue::getPendingIncidents() const {
    return std::vector<Incident>(pendingQueue_.begin(), pendingQueue_.end());
}

const Incident* IncidentQueue::peekNextIncident() const {
    if (pendingQueue_.empty()) return nullptr;
    return &(pendingQueue_.front());
}

std::pair<bool, Incident> IncidentQueue::processNextIncident() {
    if (pendingQueue_.empty()) {
        return {false, Incident{}};
    }

#ifdef SEED_BUG_5
    // Bug 5 — FIFO Queue defect:
    // Defect: Skips the first queued incident (index 0) when the queue contains multiple items,
    // selecting and dequeuing index 1 instead. This violates arrival order (FIFO) and strands
    // the oldest incident at the front of the queue!
    if (pendingQueue_.size() > 1) {
        Incident selected = pendingQueue_[1];
        pendingQueue_.erase(pendingQueue_.begin() + 1);
        selected.status = "UNDER_INVESTIGATION";
        return {true, selected};
    } else {
        Incident selected = pendingQueue_.front();
        pendingQueue_.pop_front();
        selected.status = "UNDER_INVESTIGATION";
        return {true, selected};
    }
#else
    // Correct behavior: strict FIFO arrival order
    Incident selected = pendingQueue_.front();
    pendingQueue_.pop_front();
    selected.status = "UNDER_INVESTIGATION";
    return {true, selected};
#endif
}

bool IncidentQueue::updateIncidentStatus(const std::string& incidentId,
                                        const std::string& status,
                                        const std::string& reviewNotes) {
    for (auto& inc : pendingQueue_) {
        if (inc.id == incidentId) {
            inc.status = status;
            if (!reviewNotes.empty()) inc.reviewNotes = reviewNotes;
            return true;
        }
    }
    for (auto& inc : resolvedIncidents_) {
        if (inc.id == incidentId) {
            inc.status = status;
            if (!reviewNotes.empty()) inc.reviewNotes = reviewNotes;
            return true;
        }
    }
    return false;
}

bool IncidentQueue::resolveIncident(const std::string& incidentId,
                                    const std::string& status,
                                    const std::string& reviewNotes) {
    for (auto it = pendingQueue_.begin(); it != pendingQueue_.end(); ++it) {
        if (it->id == incidentId) {
            Incident inc = *it;
            inc.status = status.empty() ? "RESOLVED" : status;
            inc.reviewNotes = reviewNotes;
            pendingQueue_.erase(it);
            resolvedIncidents_.push_back(inc);
            return true;
        }
    }
    return false;
}

const Incident* IncidentQueue::getIncident(const std::string& incidentId) const {
    for (const auto& inc : pendingQueue_) {
        if (inc.id == incidentId) return &inc;
    }
    for (const auto& inc : resolvedIncidents_) {
        if (inc.id == incidentId) return &inc;
    }
    return nullptr;
}

} // namespace apex
