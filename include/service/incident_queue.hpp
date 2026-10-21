#pragma once

#include <vector>
#include <deque>
#include <string>
#include <utility>
#include "model/race_models.hpp"

namespace apex {

class IncidentQueue {
public:
    IncidentQueue();

    void initializeDefaultIncidents();
    void reset();

    Incident reportIncident(int lap,
                            const std::string& location,
                            const std::vector<int>& involvedCars,
                            const std::string& category,
                            const std::string& severity,
                            const std::string& description);

    std::vector<Incident> getPendingIncidents() const;
    std::vector<Incident> getResolvedIncidents() const { return resolvedIncidents_; }

    const Incident* peekNextIncident() const;
    std::pair<bool, Incident> processNextIncident();

    bool updateIncidentStatus(const std::string& incidentId,
                              const std::string& status,
                              const std::string& reviewNotes);

    bool resolveIncident(const std::string& incidentId,
                         const std::string& status,
                         const std::string& reviewNotes);

    const Incident* getIncident(const std::string& incidentId) const;
    size_t getPendingCount() const { return pendingQueue_.size(); }

private:
    std::deque<Incident> pendingQueue_;
    std::vector<Incident> resolvedIncidents_;
    int sequenceCounter_ = 1;
};

} // namespace apex
