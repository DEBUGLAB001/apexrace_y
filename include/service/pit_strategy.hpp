#pragma once

#include <vector>
#include <string>
#include <utility>
#include "model/race_models.hpp"

namespace apex {

struct PitStopNode {
    PitStop data;
    PitStopNode* next = nullptr;

    PitStopNode(const PitStop& stop, PitStopNode* n = nullptr)
        : data(stop), next(n) {}
};

class PitStrategy {
public:
    PitStrategy();
    ~PitStrategy();

    PitStrategy(const PitStrategy& other);
    PitStrategy& operator=(const PitStrategy& other);

    void initializeDefaultSchedule();
    void reset();
    void clear();

    std::vector<PitStop> getOrderedStops() const;
    size_t getStopCount() const;

    std::pair<bool, std::string> validateStop(const PitStop& stop, bool isNew = true) const;
    std::pair<bool, std::string> addStop(const PitStop& stop);
    bool removeStop(const std::string& stopId);
    bool moveStopEarlier(const std::string& stopId);
    bool moveStopLater(const std::string& stopId);
    bool updateStopStatus(const std::string& stopId, const std::string& status);

    const PitStop* getStop(const std::string& stopId) const;

private:
    PitStopNode* head_ = nullptr;

    void insertNodeOrdered(PitStopNode* newNode);
};

} // namespace apex
