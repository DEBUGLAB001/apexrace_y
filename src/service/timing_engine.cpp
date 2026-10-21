#include "service/timing_engine.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace apex {

TimingEngine::TimingEngine() {
    initializeDefaultGrid();
}

void TimingEngine::initializeDefaultGrid() {
    drivers_.clear();
    driverElapsedTimes_.clear();

    struct SeedDriver {
        std::string id;
        int carNumber;
        std::string name;
        std::string team;
        double baseLap;
        double s1, s2, s3;
        double gap;
        std::string tire;
        int tireAge;
    };

    const std::vector<SeedDriver> seed = {
        {"drv_ver", 1,  "Max Verstappen",  "Red Bull Racing", 87.240, 27.120, 32.480, 27.640, 0.000, "HARD", 12},
        {"drv_nor", 4,  "Lando Norris",    "McLaren",         87.390, 27.190, 32.510, 27.690, 1.450, "MEDIUM", 8},
        {"drv_lec", 16, "Charles Leclerc", "Ferrari",         87.510, 27.240, 32.590, 27.680, 3.120, "HARD", 14},
        {"drv_ham", 44, "Lewis Hamilton",  "Mercedes",        87.650, 27.310, 32.620, 27.720, 4.890, "MEDIUM", 7},
        {"drv_pia", 81, "Oscar Piastri",   "McLaren",         87.720, 27.350, 32.650, 27.720, 6.240, "HARD", 11},
        {"drv_sai", 55, "Carlos Sainz",    "Ferrari",         87.890, 27.420, 32.710, 27.760, 8.550, "MEDIUM", 9},
        {"drv_rus", 63, "George Russell",  "Mercedes",        88.020, 27.480, 32.750, 27.790, 11.200, "HARD", 15},
        {"drv_per", 11, "Sergio Perez",    "Red Bull Racing", 88.210, 27.530, 32.840, 27.840, 14.650, "SOFT", 5},
        {"drv_alo", 14, "Fernando Alonso", "Aston Martin",    88.450, 27.620, 32.910, 27.920, 17.820, "HARD", 16},
        {"drv_gas", 10, "Pierre Gasly",    "Alpine",          88.780, 27.750, 33.020, 28.010, 21.400, "MEDIUM", 10},
        {"drv_alb", 23, "Alex Albon",      "Williams",        88.950, 27.810, 33.090, 28.050, 24.150, "HARD", 13},
        {"drv_hul", 27, "Nico Hulkenberg", "Haas",            89.180, 27.900, 33.180, 28.100, 27.800, "MEDIUM", 6}
    };

    double leaderTotalTime = 16 * 87.500; // ~1400 seconds

    for (size_t i = 0; i < seed.size(); ++i) {
        const auto& s = seed[i];
        Driver d;
        d.id = s.id;
        d.carNumber = s.carNumber;
        d.name = s.name;
        d.team = s.team;
        d.position = static_cast<int>(i + 1);
        d.previousPosition = static_cast<int>(i + 1);
        d.completedLaps = 16;
        d.lastLapTime = s.baseLap;
        d.bestLapTime = s.baseLap - 0.350;
        d.s1Time = s.s1;
        d.s2Time = s.s2;
        d.s3Time = s.s3;
        d.gapToLeader = s.gap;
        d.intervalToAhead = (i == 0) ? 0.0 : (s.gap - seed[i - 1].gap);
        d.pitStatus = "ON TRACK";
        d.tireCompound = s.tire;
        d.tireAgeLaps = s.tireAge;

        // Seed 4 previous lap records for telemetry/history panel
        for (int lap = 13; lap <= 16; ++lap) {
            LapRecord lr;
            lr.lapNumber = lap;
            double variance = (lap == 16) ? 0.0 : ((lap % 2 == 0) ? 0.220 : -0.150);
            lr.lapTime = s.baseLap + variance;
            lr.s1 = s.s1 + variance * 0.3;
            lr.s2 = s.s2 + variance * 0.4;
            lr.s3 = s.s3 + variance * 0.3;
            lr.isPersonalBest = (lr.lapTime <= d.bestLapTime + 0.05);
            lr.isOverallBest = (i == 0 && lr.isPersonalBest);
            d.lapHistory.push_back(lr);
        }

        drivers_.push_back(d);
        driverElapsedTimes_.push_back(leaderTotalTime + s.gap);
    }

    sessionState_.eventName = "Apex British Grand Prix";
    sessionState_.circuit = "Silverstone Circuit";
    sessionState_.circuitLengthKm = 5.891;
    sessionState_.sessionType = "Race";
    sessionState_.sessionClockSeconds = 1425;
    sessionState_.currentLap = 16;
    sessionState_.totalLaps = 52;
    sessionState_.trackStatus = "GREEN";
    sessionState_.isSimRunning = false;
    sessionState_.eventLogs = {
        "14:00:00 - Session initiated: GREEN FLAG.",
        "14:08:12 - Car #16 set fastest lap: 1:27.160.",
        "14:15:30 - Car #1 set new overall fastest lap: 1:26.890.",
        "14:22:15 - Pit window open for Medium tire starters."
    };

    recalculateLeaderboard();
}

void TimingEngine::reset() {
    initializeDefaultGrid();
}

const Driver* TimingEngine::getDriver(const std::string& id) const {
    for (const auto& d : drivers_) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

Driver* TimingEngine::getDriver(const std::string& id) {
    for (auto& d : drivers_) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

const Driver* TimingEngine::getDriverByNumber(int carNumber) const {
    for (const auto& d : drivers_) {
        if (d.carNumber == carNumber) return &d;
    }
    return nullptr;
}

Driver* TimingEngine::getDriverByNumber(int carNumber) {
    for (auto& d : drivers_) {
        if (d.carNumber == carNumber) return &d;
    }
    return nullptr;
}

void TimingEngine::updateFastestLap() {
    double bestTime = 999999.0;
    std::string fastestDriverId = "";

    for (const auto& d : drivers_) {
        if (d.bestLapTime > 0.0 && d.bestLapTime < bestTime) {
            bestTime = d.bestLapTime;
            fastestDriverId = d.id;
        }
    }

    for (auto& d : drivers_) {
        d.isFastestLap = (!fastestDriverId.empty() && d.id == fastestDriverId);
    }
}

void TimingEngine::recalculateLeaderboard() {
    if (drivers_.empty()) return;

    // Preserve previous positions before sorting
    for (auto& d : drivers_) {
        d.previousPosition = d.position;
    }

    // Pair drivers with their elapsed times for sorting
    std::vector<std::pair<Driver, double>> pairs;
    pairs.reserve(drivers_.size());
    for (size_t i = 0; i < drivers_.size(); ++i) {
        pairs.emplace_back(drivers_[i], driverElapsedTimes_[i]);
    }

    // Sort by status (non-disqualified first), completed laps descending, then elapsed time + penalties ascending
    std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
        if (a.first.isDisqualified != b.first.isDisqualified) {
            return !a.first.isDisqualified;
        }
        if (a.first.completedLaps != b.first.completedLaps) {
            return a.first.completedLaps > b.first.completedLaps;
        }
        double totalA = a.second + a.first.totalPenaltySeconds;
        double totalB = b.second + b.first.totalPenaltySeconds;
        return totalA < totalB;
    });

    for (size_t i = 0; i < pairs.size(); ++i) {
        drivers_[i] = pairs[i].first;
        driverElapsedTimes_[i] = pairs[i].second;
        drivers_[i].position = static_cast<int>(i + 1);
    }

    // Leader has 0.0 gap
    drivers_[0].gapToLeader = 0.0;
    drivers_[0].intervalToAhead = 0.0;

#ifdef SEED_BUG_1
    // Bug 1 — Array Indexing:
    // Off-by-one defect: loop bounds check uses `i < drivers_.size() - 1`
    // This erroneously omits the boundary driver (the last driver in the leaderboard, P12)!
    for (size_t i = 1; i < drivers_.size() - 1; ++i) {
#else
    // Correct behavior: all valid entries from 1 to drivers_.size()-1 are calculated
    for (size_t i = 1; i < drivers_.size(); ++i) {
#endif
        double leaderTotal = driverElapsedTimes_[0] + drivers_[0].totalPenaltySeconds;
        double currentTotal = driverElapsedTimes_[i] + drivers_[i].totalPenaltySeconds;
        double prevTotal = driverElapsedTimes_[i - 1] + drivers_[i - 1].totalPenaltySeconds;

        drivers_[i].gapToLeader = currentTotal - leaderTotal;
        drivers_[i].intervalToAhead = currentTotal - prevTotal;
    }

    updateFastestLap();
}

bool TimingEngine::recordLap(const std::string& driverId, double lapTime, double s1, double s2, double s3) {
    Driver* d = getDriver(driverId);
    if (!d) return false;

    d->completedLaps += 1;
    d->lastLapTime = lapTime;
    d->s1Time = s1;
    d->s2Time = s2;
    d->s3Time = s3;
    d->tireAgeLaps += 1;

    bool isPb = false;
    if (d->bestLapTime <= 0.0 || lapTime < d->bestLapTime) {
        d->bestLapTime = lapTime;
        isPb = true;
    }

    LapRecord rec;
    rec.lapNumber = d->completedLaps;
    rec.lapTime = lapTime;
    rec.s1 = s1;
    rec.s2 = s2;
    rec.s3 = s3;
    rec.isPersonalBest = isPb;
    rec.isOverallBest = false;
    d->lapHistory.push_back(rec);

    // Update elapsed time
    for (size_t i = 0; i < drivers_.size(); ++i) {
        if (drivers_[i].id == driverId) {
            driverElapsedTimes_[i] += lapTime;
            break;
        }
    }

    recalculateLeaderboard();
    return true;
}

void TimingEngine::advanceSessionLap() {
    sessionState_.currentLap += 1;
    sessionState_.sessionClockSeconds += 88;

    // Deterministic simulation update across all drivers
    for (size_t i = 0; i < drivers_.size(); ++i) {
        auto& d = drivers_[i];
        if (d.isDisqualified) continue;

        // Deterministic variation based on car number and current lap
        double delta = std::sin(sessionState_.currentLap * 0.7 + d.carNumber) * 0.35;
        double lapTime = 87.0 + (i * 0.15) + delta;
        double s1 = 27.0 + (lapTime - 87.0) * 0.31;
        double s2 = 32.4 + (lapTime - 87.0) * 0.38;
        double s3 = lapTime - s1 - s2;

        d.completedLaps += 1;
        d.lastLapTime = lapTime;
        d.s1Time = s1;
        d.s2Time = s2;
        d.s3Time = s3;
        d.tireAgeLaps += 1;

        if (lapTime < d.bestLapTime || d.bestLapTime <= 0.0) {
            d.bestLapTime = lapTime;
        }

        LapRecord rec;
        rec.lapNumber = sessionState_.currentLap;
        rec.lapTime = lapTime;
        rec.s1 = s1;
        rec.s2 = s2;
        rec.s3 = s3;
        rec.isPersonalBest = (lapTime <= d.bestLapTime);
        rec.isOverallBest = false;
        d.lapHistory.push_back(rec);

        driverElapsedTimes_[i] += lapTime;
    }

    recalculateLeaderboard();

    std::ostringstream ss;
    ss << "Lap " << sessionState_.currentLap << " completed. P1: Car #"
       << drivers_[0].carNumber << " (" << drivers_[0].name << ")";
    addSessionLog(ss.str());
}

void TimingEngine::setTrackStatus(const std::string& status) {
    sessionState_.trackStatus = status;
    addSessionLog("Track status changed to: " + status);
}

void TimingEngine::setSimRunning(bool running) {
    sessionState_.isSimRunning = running;
}

void TimingEngine::addSessionLog(const std::string& message) {
    sessionState_.eventLogs.push_back(message);
    if (sessionState_.eventLogs.size() > 50) {
        sessionState_.eventLogs.erase(sessionState_.eventLogs.begin());
    }
}

} // namespace apex
