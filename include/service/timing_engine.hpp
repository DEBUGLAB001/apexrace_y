#pragma once

#include <vector>
#include <string>
#include <memory>
#include "model/race_models.hpp"

namespace apex {

class TimingEngine {
public:
    TimingEngine();

    void initializeDefaultGrid();
    void reset();

    const std::vector<Driver>& getDrivers() const { return drivers_; }
    std::vector<Driver>& getDrivers() { return drivers_; }

    const Driver* getDriver(const std::string& id) const;
    Driver* getDriver(const std::string& id);

    const Driver* getDriverByNumber(int carNumber) const;
    Driver* getDriverByNumber(int carNumber);

    bool recordLap(const std::string& driverId, double lapTime, double s1, double s2, double s3);
    void recalculateLeaderboard();
    void advanceSessionLap();

    SessionState& getSessionState() { return sessionState_; }
    const SessionState& getSessionState() const { return sessionState_; }

    void setTrackStatus(const std::string& status);
    void setSimRunning(bool running);
    void addSessionLog(const std::string& message);

private:
    std::vector<Driver> drivers_;
    SessionState sessionState_;
    std::vector<double> driverElapsedTimes_;

    void updateFastestLap();
};

} // namespace apex
