#include "service/pit_strategy.hpp"
#include <algorithm>

namespace apex {

PitStrategy::PitStrategy() {
    initializeDefaultSchedule();
}

PitStrategy::~PitStrategy() {
    clear();
}

PitStrategy::PitStrategy(const PitStrategy& other) {
    clear();
    PitStopNode* curr = other.head_;
    PitStopNode** dest = &head_;
    while (curr) {
        *dest = new PitStopNode(curr->data);
        dest = &((*dest)->next);
        curr = curr->next;
    }
}

PitStrategy& PitStrategy::operator=(const PitStrategy& other) {
    if (this != &other) {
        clear();
        PitStopNode* curr = other.head_;
        PitStopNode** dest = &head_;
        while (curr) {
            *dest = new PitStopNode(curr->data);
            dest = &((*dest)->next);
            curr = curr->next;
        }
    }
    return *this;
}

void PitStrategy::clear() {
    PitStopNode* curr = head_;
    while (curr) {
        PitStopNode* next = curr->next;
        delete curr;
        curr = next;
    }
    head_ = nullptr;
}

void PitStrategy::initializeDefaultSchedule() {
    clear();

    addStop({"pit_001", 1, 20, "HARD", "SCHEDULED"});
    addStop({"pit_002", 4, 22, "HARD", "SCHEDULED"});
    addStop({"pit_003", 16, 24, "MEDIUM", "SCHEDULED"});
    addStop({"pit_004", 44, 27, "HARD", "SCHEDULED"});
    addStop({"pit_005", 81, 29, "MEDIUM", "SCHEDULED"});
}

void PitStrategy::reset() {
    initializeDefaultSchedule();
}

std::vector<PitStop> PitStrategy::getOrderedStops() const {
    std::vector<PitStop> result;
    PitStopNode* curr = head_;
    while (curr) {
        result.push_back(curr->data);
        curr = curr->next;
    }
    return result;
}

size_t PitStrategy::getStopCount() const {
    size_t count = 0;
    PitStopNode* curr = head_;
    while (curr) {
        ++count;
        curr = curr->next;
    }
    return count;
}

const PitStop* PitStrategy::getStop(const std::string& stopId) const {
    PitStopNode* curr = head_;
    while (curr) {
        if (curr->data.id == stopId) {
            return &(curr->data);
        }
        curr = curr->next;
    }
    return nullptr;
}

std::pair<bool, std::string> PitStrategy::validateStop(const PitStop& stop, bool /*isNew*/) const {
    if (stop.carNumber <= 0) {
        return {false, "Validation error: Car number must be greater than 0"};
    }
    if (stop.plannedLap < 1 || stop.plannedLap > 70) {
        return {false, "Validation error: Planned lap must be between 1 and 70"};
    }
    if (stop.stopType.empty()) {
        return {false, "Validation error: Stop type cannot be empty"};
    }

#ifdef SEED_BUG_4
    // Bug 4 — Pit Stop Ordering / Scheduling defect:
    // Defect: Erroneously forbids multiple cars from stopping on the same lap by checking
    // global lap uniqueness across the entire grid instead of per-car!
    PitStopNode* curr = head_;
    while (curr) {
        if (curr->data.id != stop.id && curr->data.plannedLap == stop.plannedLap) {
            return {false, "Validation error: Lap " + std::to_string(stop.plannedLap) + " already has a scheduled pit stop"};
        }
        curr = curr->next;
    }
#else
    // Correct behavior: Multiple different cars can pit on the same lap (e.g. double stacking or neutral safety car pit windows),
    // but the SAME car cannot have two stops planned on the exact same lap.
    PitStopNode* curr = head_;
    while (curr) {
        if (curr->data.id != stop.id && curr->data.carNumber == stop.carNumber && curr->data.plannedLap == stop.plannedLap) {
            return {false, "Validation error: Car #" + std::to_string(stop.carNumber) + " already has a planned stop on lap " + std::to_string(stop.plannedLap)};
        }
        curr = curr->next;
    }
#endif

    return {true, ""};
}

void PitStrategy::insertNodeOrdered(PitStopNode* newNode) {
    if (!head_ || newNode->data.plannedLap < head_->data.plannedLap) {
        newNode->next = head_;
        head_ = newNode;
        return;
    }

    PitStopNode* curr = head_;
    while (curr->next && curr->next->data.plannedLap <= newNode->data.plannedLap) {
        curr = curr->next;
    }

    newNode->next = curr->next;
    curr->next = newNode;
}

std::pair<bool, std::string> PitStrategy::addStop(const PitStop& stop) {
    auto val = validateStop(stop, true);
    if (!val.first) {
        return val;
    }

    PitStop s = stop;
    if (s.id.empty()) {
        s.id = "pit_" + std::to_string(getStopCount() + 100);
    }

    PitStopNode* newNode = new PitStopNode(s);
    insertNodeOrdered(newNode);
    return {true, s.id};
}

bool PitStrategy::removeStop(const std::string& stopId) {
    if (!head_) return false;

    if (head_->data.id == stopId) {
        PitStopNode* toDelete = head_;
        head_ = head_->next;
        delete toDelete;
        return true;
    }

    PitStopNode* prev = head_;
    PitStopNode* curr = head_->next;

    while (curr) {
        if (curr->data.id == stopId) {
            PitStopNode* toDelete = curr;
#ifdef SEED_BUG_3
            // Bug 3 — Linked List Traversal / Deletion defect:
            // Incorrect pointer update skips curr->next, inadvertently dropping the subsequent node from the list!
            prev->next = (curr->next != nullptr) ? curr->next->next : nullptr;
#else
            // Correct behavior: properly link predecessor to curr->next
            prev->next = curr->next;
#endif
            delete toDelete;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    return false;
}

bool PitStrategy::moveStopEarlier(const std::string& stopId) {
    if (!head_ || head_->data.id == stopId) {
        return false; // Already at the front or empty
    }

    // If second node moves to head
    if (head_->next && head_->next->data.id == stopId) {
        PitStopNode* second = head_->next;
        head_->next = second->next;
        second->next = head_;
        head_ = second;
        return true;
    }

    PitStopNode* prevPrev = head_;
    PitStopNode* prev = head_->next;
    PitStopNode* curr = prev ? prev->next : nullptr;

    while (curr) {
        if (curr->data.id == stopId) {
            prev->next = curr->next;
            curr->next = prev;
            prevPrev->next = curr;
            return true;
        }
        prevPrev = prev;
        prev = curr;
        curr = curr->next;
    }

    return false;
}

bool PitStrategy::moveStopLater(const std::string& stopId) {
    if (!head_) return false;

    if (head_->data.id == stopId) {
        if (!head_->next) return false; // Already at tail
        PitStopNode* second = head_->next;
        head_->next = second->next;
        second->next = head_;
        head_ = second;
        return true;
    }

    PitStopNode* prev = head_;
    PitStopNode* curr = head_->next;

    while (curr && curr->next) {
        if (curr->data.id == stopId) {
            PitStopNode* next = curr->next;
            curr->next = next->next;
            next->next = curr;
            prev->next = next;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    return false;
}

bool PitStrategy::updateStopStatus(const std::string& stopId, const std::string& status) {
    PitStopNode* curr = head_;
    while (curr) {
        if (curr->data.id == stopId) {
            curr->data.status = status;
            return true;
        }
        curr = curr->next;
    }
    return false;
}

} // namespace apex
