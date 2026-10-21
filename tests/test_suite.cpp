#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include "json.hpp"
#include "service/timing_engine.hpp"
#include "service/radio_parser.hpp"
#include "service/pit_strategy.hpp"
#include "service/incident_queue.hpp"
#include "service/steward_decisions.hpp"

using json = nlohmann::json;
using namespace apex;

struct TestResult {
    std::string name;
    bool passed = false;
    int durationMs = 0;
    std::string message;
};

// ==========================================
// TARGETED TESTS FOR THE 6 DEFECT TOPICS
// ==========================================

// Bug 1: Array Indexing — Leaderboard boundary driver calculation
bool test_array_indexing_leaderboard_boundary(std::string& msg) {
    TimingEngine engine;
    engine.recalculateLeaderboard();
    const auto& drivers = engine.getDrivers();

    if (drivers.size() < 12) {
        msg = "Expected at least 12 seeded drivers, found: " + std::to_string(drivers.size());
        return false;
    }

    // The boundary driver is the last driver in the leaderboard (index size()-1, P12)
    const auto& lastDriver = drivers.back();
    if (lastDriver.position != static_cast<int>(drivers.size())) {
        msg = "Last driver position mismatch. Expected " + std::to_string(drivers.size()) +
              ", got " + std::to_string(lastDriver.position);
        return false;
    }

    // In a race, the last driver behind leader must have a positive gap to leader (> 0.0)
    // If the loop omitted the boundary driver (i < size - 1), gap remains 0.0
    if (lastDriver.gapToLeader <= 0.0) {
        msg = "Defect detected: Boundary driver at position " + std::to_string(lastDriver.position) +
              " (" + lastDriver.name + ") has uncalculated gapToLeader: " +
              std::to_string(lastDriver.gapToLeader);
        return false;
    }

    if (lastDriver.intervalToAhead <= 0.0) {
        msg = "Defect detected: Boundary driver has uncalculated intervalToAhead";
        return false;
    }

    msg = "Boundary driver " + lastDriver.name + " (P" + std::to_string(lastDriver.position) +
          ") gap correctly computed: +" + std::to_string(lastDriver.gapToLeader) + "s";
    return true;
}

// Bug 2: String Processing — Case-insensitive radio keyword search
bool test_string_processing_radio_keyword_search(std::string& msg) {
    RadioParser parser;

    // "rad_001" contains "Box this lap, box for hard tires."
    // "rad_008" contains "Debris on main straight near pit exit."
    // User searches with uppercase / mixed case
    auto resultsBox = parser.filterMessages("BOX", "ALL", 0);
    if (resultsBox.empty()) {
        msg = "Defect detected: Searching for 'BOX' failed to match message containing 'box'";
        return false;
    }

    auto resultsDebris = parser.filterMessages("DEBRIS", "ALL", 0);
    if (resultsDebris.empty()) {
        msg = "Defect detected: Searching for 'DEBRIS' failed to match message containing 'Debris'";
        return false;
    }

    auto resultsPuncture = parser.filterMessages("Tire", "ALL", 0);
    if (resultsPuncture.empty()) {
        msg = "Defect detected: Searching for 'Tire' failed to match messages with 'tire'";
        return false;
    }

    msg = "Case-insensitive searches with uppercase query strings matched properly";
    return true;
}

// Bug 3: Linked List Traversal — Stop removal preserves adjacent nodes
bool test_linked_list_traversal_stop_removal(std::string& msg) {
    PitStrategy strategy;
    // Default has: pit_001 (lap 20), pit_002 (lap 22), pit_003 (lap 24), pit_004 (lap 27), pit_005 (lap 29)
    size_t initialCount = strategy.getStopCount();
    if (initialCount < 5) {
        msg = "Unexpected initial stop count: " + std::to_string(initialCount);
        return false;
    }

    // Removing the second node (pit_002)
    bool removed = strategy.removeStop("pit_002");
    if (!removed) {
        msg = "Failed to remove stop pit_002";
        return false;
    }

    // If Bug 3 is present, pit_003 was accidentally dropped along with pit_002!
    const PitStop* pit003 = strategy.getStop("pit_003");
    if (!pit003) {
        msg = "Defect detected: Removing pit_002 caused adjacent node pit_003 to be dropped from linked list!";
        return false;
    }

    if (strategy.getStopCount() != initialCount - 1) {
        msg = "Defect detected: Stop count after removing 1 stop should be " +
              std::to_string(initialCount - 1) + ", but got " + std::to_string(strategy.getStopCount());
        return false;
    }

    msg = "Linked list removal correctly preserved adjacent nodes and list integrity";
    return true;
}

// Bug 4: Pit Stop Ordering — Multiple cars allowed to pit on the same lap
bool test_pit_stop_scheduling_multi_car_same_lap(std::string& msg) {
    PitStrategy strategy;
    // Default schedule has Car #1 on lap 20 (pit_001)

    // Schedule a stop on the SAME lap 20 for a DIFFERENT car (e.g. Car #44)
    PitStop stopCar44;
    stopCar44.id = "pit_test_44";
    stopCar44.carNumber = 44;
    stopCar44.plannedLap = 20;
    stopCar44.stopType = "HARD";
    stopCar44.status = "SCHEDULED";

    auto addResult = strategy.addStop(stopCar44);
    if (!addResult.first) {
        msg = "Defect detected: Valid pit stop on lap 20 for Car #44 was rejected because Car #1 also pits on lap 20! Reason: " + addResult.second;
        return false;
    }

    // Now test that the SAME car (#44) cannot have a second stop on lap 20
    PitStop duplicateCar44;
    duplicateCar44.id = "pit_test_44_dup";
    duplicateCar44.carNumber = 44;
    duplicateCar44.plannedLap = 20;
    duplicateCar44.stopType = "SOFT";
    duplicateCar44.status = "SCHEDULED";

    auto dupResult = strategy.addStop(duplicateCar44);
    if (dupResult.first) {
        msg = "Defect: Duplicate planned stop on lap 20 for the same Car #44 was erroneously accepted";
        return false;
    }

    msg = "Pit stop scheduling correctly permits multiple cars on the same lap while preventing same-car duplicates";
    return true;
}

// Bug 5: FIFO Queue — Incident arrival order processing
bool test_fifo_queue_incident_order(std::string& msg) {
    IncidentQueue queue;
    // Clear and report 3 incidents
    queue.reset();

    // The default queue has 3 pending incidents: inc_001, inc_002, inc_003
    auto pending = queue.getPendingIncidents();
    if (pending.size() < 3) {
        msg = "Expected at least 3 pending incidents, found " + std::to_string(pending.size());
        return false;
    }

    std::string expectedFirstId = pending.front().id; // "inc_001"

    auto processResult = queue.processNextIncident();
    if (!processResult.first) {
        msg = "Failed to process next incident from non-empty queue";
        return false;
    }

    if (processResult.second.id != expectedFirstId) {
        msg = "Defect detected: FIFO queue violation! Expected oldest incident '" +
              expectedFirstId + "' to be processed first, but got '" + processResult.second.id + "'";
        return false;
    }

    msg = "FIFO incident queue correctly processed incidents in arrival order";
    return true;
}

// Bug 6: Stack-Based Undo — Penalty restoration for driver
bool test_stack_based_undo_driver_penalty_restoration(std::string& msg) {
    TimingEngine engine;
    StewardDecisions steward;

    Driver* d = engine.getDriverByNumber(1);
    if (!d) {
        msg = "Driver Car #1 not found";
        return false;
    }

    int initialPenalties = d->totalPenaltySeconds; // 0
    int initialWarnings = d->warningsCount;

    // Apply 5-second time penalty to Car #1
    StewardDecision penalty;
    penalty.incidentId = "inc_test_01";
    penalty.carNumber = 1;
    penalty.driverId = d->id;
    penalty.actionType = "TIME_PENALTY";
    penalty.penaltySeconds = 5;
    penalty.reason = "Track limits violation";
    penalty.reversible = true;

    auto applyRes = steward.applyDecision(penalty, engine);
    if (!applyRes.first) {
        msg = "Failed to apply penalty decision: " + applyRes.second;
        return false;
    }

    // Re-fetch driver pointer because leaderboard sorting may have moved the driver's position in the vector
    d = engine.getDriverByNumber(1);
    if (!d || d->totalPenaltySeconds != initialPenalties + 5) {
        msg = "Penalty was not applied to driver totalPenaltySeconds";
        return false;
    }

    // Now undo the decision
    auto undoRes = steward.undoLatestDecision(engine);
    if (!undoRes.first) {
        msg = "Undo latest decision failed";
        return false;
    }

    // Re-fetch driver pointer after undo resorts leaderboard
    d = engine.getDriverByNumber(1);
    if (!d || d->totalPenaltySeconds != initialPenalties) {
        msg = "Defect detected: Undo popped decision but failed to restore totalPenaltySeconds! Expected " +
              std::to_string(initialPenalties) + ", driver still has: " +
              (d ? std::to_string(d->totalPenaltySeconds) : "null");
        return false;
    }

    if (d->warningsCount != initialWarnings) {
        msg = "Warnings count was not restored properly";
        return false;
    }

    msg = "Stack-based undo correctly and completely restored driver penalty state";
    return true;
}

// ==========================================
// BASELINE TESTS FOR NORMAL BEHAVIOR
// ==========================================

bool test_baseline_driver_grid_initialization(std::string& msg) {
    TimingEngine engine;
    const auto& drivers = engine.getDrivers();
    if (drivers.size() != 12) {
        msg = "Expected exactly 12 drivers, got " + std::to_string(drivers.size());
        return false;
    }

    // Verify positions 1 to 12
    for (size_t i = 0; i < drivers.size(); ++i) {
        if (drivers[i].position != static_cast<int>(i + 1)) {
            msg = "Driver position order invalid at index " + std::to_string(i);
            return false;
        }
    }
    msg = "Grid properly initialized with 12 drivers";
    return true;
}

bool test_baseline_record_lap_telemetry(std::string& msg) {
    TimingEngine engine;
    bool ok = engine.recordLap("drv_ver", 86.950, 26.900, 32.200, 27.850);
    if (!ok) {
        msg = "recordLap failed for drv_ver";
        return false;
    }
    const Driver* d = engine.getDriver("drv_ver");
    if (!d || d->lastLapTime != 86.950) {
        msg = "Driver last lap time was not updated";
        return false;
    }
    msg = "Lap record telemetry successfully recorded";
    return true;
}

bool test_baseline_session_lap_advance(std::string& msg) {
    TimingEngine engine;
    int initialLap = engine.getSessionState().currentLap;
    engine.advanceSessionLap();
    if (engine.getSessionState().currentLap != initialLap + 1) {
        msg = "Session lap did not advance by 1";
        return false;
    }
    msg = "Session lap successfully advanced";
    return true;
}

bool test_baseline_incident_reporting(std::string& msg) {
    IncidentQueue queue;
    size_t countBefore = queue.getPendingCount();
    Incident inc = queue.reportIncident(20, "Turn 1", {4, 16}, "COLLISION", "HIGH", "Contact at apex");
    if (inc.id.empty() || queue.getPendingCount() != countBefore + 1) {
        msg = "Incident was not added to pending queue";
        return false;
    }
    msg = "Incident reporting successfully enqueued";
    return true;
}

bool test_baseline_pit_stop_ordering_chronological(std::string& msg) {
    PitStrategy strategy;
    strategy.clear();

    strategy.addStop({"p1", 1, 30, "HARD", "SCHEDULED"});
    strategy.addStop({"p2", 4, 15, "MEDIUM", "SCHEDULED"});
    strategy.addStop({"p3", 16, 22, "HARD", "SCHEDULED"});

    auto stops = strategy.getOrderedStops();
    if (stops.size() != 3) {
        msg = "Expected 3 stops in ordered list";
        return false;
    }

    if (stops[0].plannedLap != 15 || stops[1].plannedLap != 22 || stops[2].plannedLap != 30) {
        msg = "Pit stops were not sorted in ascending order of planned lap";
        return false;
    }

    msg = "Pit stop list sorted chronologically by planned lap";
    return true;
}

bool test_baseline_steward_warning_application(std::string& msg) {
    TimingEngine engine;
    StewardDecisions steward;

    Driver* d = engine.getDriverByNumber(44);
    int prevWarnings = d->warningsCount;

    StewardDecision warn;
    warn.carNumber = 44;
    warn.actionType = "WARNING";
    warn.reason = "Impeding in qualifying sector";
    steward.applyDecision(warn, engine);

    if (d->warningsCount != prevWarnings + 1) {
        msg = "Warning count was not incremented";
        return false;
    }
    msg = "Warning successfully applied to driver";
    return true;
}

// ==========================================
// MAIN TEST HARNESS RUNNER
// ==========================================

int main() {
    struct TestCase {
        std::string name;
        bool (*func)(std::string&);
    };

    std::vector<TestCase> tests = {
        // Targeted tests for 6 bug topics
        {"Targeted: Array Indexing (Leaderboard Boundary Driver)", test_array_indexing_leaderboard_boundary},
        {"Targeted: String Processing (Radio Keyword Search)", test_string_processing_radio_keyword_search},
        {"Targeted: Linked List Traversal (Stop Removal Integrity)", test_linked_list_traversal_stop_removal},
        {"Targeted: Pit Stop Scheduling (Multi-Car Same Lap)", test_pit_stop_scheduling_multi_car_same_lap},
        {"Targeted: FIFO Queue (Incident Arrival Order)", test_fifo_queue_incident_order},
        {"Targeted: Stack-Based Undo (Driver Penalty Restoration)", test_stack_based_undo_driver_penalty_restoration},

        // Baseline tests
        {"Baseline: Driver Grid Initialization", test_baseline_driver_grid_initialization},
        {"Baseline: Record Lap Telemetry", test_baseline_record_lap_telemetry},
        {"Baseline: Session Lap Advance", test_baseline_session_lap_advance},
        {"Baseline: Incident Reporting", test_baseline_incident_reporting},
        {"Baseline: Pit Stop Chronological Order", test_baseline_pit_stop_ordering_chronological},
        {"Baseline: Steward Warning Application", test_baseline_steward_warning_application}
    };

    int passed = 0;
    int failed = 0;
    json testOutputs = json::array();

    auto startTime = std::chrono::high_resolution_clock::now();

    for (const auto& test : tests) {
        auto t0 = std::chrono::high_resolution_clock::now();
        std::string detail;
        bool ok = false;
        try {
            ok = test.func(detail);
        } catch (const std::exception& e) {
            ok = false;
            detail = std::string("Unhandled exception: ") + e.what();
        } catch (...) {
            ok = false;
            detail = "Unhandled non-std exception";
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

        if (ok) {
            ++passed;
            std::cerr << "[PASS] " << test.name << " (" << ms << "ms) - " << detail << std::endl;
        } else {
            ++failed;
            std::cerr << "[FAIL] " << test.name << " (" << ms << "ms) - " << detail << std::endl;
        }

        testOutputs.push_back({
            {"name", test.name},
            {"status", ok ? "PASSED" : "FAILED"},
            {"duration_ms", ms},
            {"message", detail}
        });
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    int totalMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

    json finalReport = {
        {"total", static_cast<int>(tests.size())},
        {"passed", passed},
        {"failed", failed},
        {"duration_ms", totalMs},
        {"results", testOutputs}
    };

    // Print JSON report to stdout only
    std::cout << finalReport.dump(2) << std::endl;

    return (failed == 0) ? 0 : 1;
}
