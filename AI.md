# ApexRace

## Description
ApexRace is a deterministic motorsport race-control simulation application built in C++17 with a lightweight embedded HTTP server (`cpp-httplib` in `include/httplib.h`), JSON serialization (`nlohmann/json` in `include/json.hpp`), and a responsive multi-page web frontend (`public/`). The backend entry point in `src/main.cpp` mounts static assets and exposes REST endpoints connecting five core domain services: `TimingEngine` (`src/service/timing_engine.cpp`) manages driver timing, lap telemetry, and leaderboard rankings; `RadioParser` (`src/service/radio_parser.cpp`) handles transmission filtering and string normalization; `PitStrategy` (`src/service/pit_strategy.cpp`) maintains an ordered plan using a custom singly linked list; `IncidentQueue` (`src/service/incident_queue.cpp`) routes incoming track incidents through an arrival-order FIFO queue; and `StewardDecisions` (`src/service/steward_decisions.cpp`) executes driver penalties with a stack-based reversible undo mechanism. Domain models and JSON bindings are centralized in `include/model/race_models.hpp`.

## Repository Structure
```
.
├── challenge.json               # Platform runtime configuration
├── CMakeLists.txt               # CMake build definitions (C++17)
├── start.sh                     # Server startup and incremental rebuild watcher
├── include/
│   ├── httplib.h                # Embedded HTTP server library
│   ├── json.hpp                 # Embedded JSON parser library
│   ├── model/
│   │   └── race_models.hpp      # Domain models (Driver, Incident, PitStop, etc.)
│   └── service/
│       ├── timing_engine.hpp    # Timing and leaderboard service interface
│       ├── radio_parser.hpp     # Radio message and search interface
│       ├── pit_strategy.hpp     # Pit stop linked list interface
│       ├── incident_queue.hpp   # Incident FIFO queue interface
│       └── steward_decisions.hpp# Steward decisions and undo stack interface
├── src/
│   ├── main.cpp                 # Application bootstrap and HTTP route handlers
│   └── service/
│       ├── timing_engine.cpp    # Timing calculation and leaderboard sorting
│       ├── radio_parser.cpp     # Message filtering and keyword normalization
│       ├── pit_strategy.cpp     # Singly linked list stop scheduling and reordering
│       ├── incident_queue.cpp   # FIFO queue processing and incident review
│       └── steward_decisions.cpp# Sanction execution and reversible state restoration
└── public/
    ├── index.html               # Multi-page race control console layout
    ├── css/style.css            # Station UI styling and telemetry tables
    └── js/app.js                # Frontend API client and view router
```

## Bugs and Bug Locations

1. Array Indexing on Leaderboard Boundary Driver
- Bug location: `src/service/timing_engine.cpp`, within `TimingEngine::recalculateLeaderboard()`.
- How to observe it: Navigate to the Live Timing Tower or call `GET /api/timing` with 12 drivers on the grid.
- Failure: The gap calculation loop condition `i < drivers_.size() - 1` stops prematurely, omitting the boundary driver at position 12 (Nico Hulkenberg), leaving their gap to leader and interval uncalculated at 0.0.
- Expected: Loop must iterate `i < drivers_.size()`, ensuring every driver including the boundary entry receives accurate gap and interval calculations.

2. String Processing in Case-Insensitive Radio Keyword Search
- Bug location: `src/service/radio_parser.cpp`, within `RadioParser::normalizeString()`.
- How to observe it: Query `GET /api/radio?query=BOX` or type "BOX" in uppercase into the search filter on the Race Radio page.
- Failure: Inconsistent normalization lowercases only `trimmed[0]`, leaving subsequent characters uppercase so queries like "BOX" fail to match "box".
- Expected: All characters in both query and message text must be converted to lowercase via `std::tolower`, allowing case-insensitive matching regardless of query casing.

3. Linked List Traversal in Pit Stop Removal
- Bug location: `src/service/pit_strategy.cpp`, within `PitStrategy::removeStop()`.
- How to observe it: Remove `pit_002` (Car #4) from the Pit Strategy Wall via `DELETE /api/pit-strategy/stops/pit_002` when 5 scheduled stops exist.
- Failure: Pointer reassignment `prev->next = curr->next ? curr->next->next : nullptr` skips the successor node (`pit_003`), accidentally dropping it along with `pit_002`.
- Expected: Unlinking must set `prev->next = curr->next`, deleting only the target stop and preserving adjacent list nodes.

4. Pit Stop Scheduling Lap Collision Check
- Bug location: `src/service/pit_strategy.cpp`, within `PitStrategy::validateStop()`.
- How to observe it: Submit a new pit stop for Car #44 on Lap 20 when Car #1 is already scheduled to pit on Lap 20.
- Failure: The validation loop rejects the stop because it incorrectly enforces a global unique planned lap across all cars rather than checking for duplicate stops by the same car.
- Expected: Multiple different cars must be permitted to pit on the same lap, rejecting additions only if the same car already has a planned stop on that lap.

5. Incident Dispatch Queue Arrival Order
- Bug location: `src/service/incident_queue.cpp`, within `IncidentQueue::processNextIncident()`.
- How to observe it: Enqueue three incidents (INC-001, INC-002, INC-003) and call `POST /api/incidents/process-next` or click "REVIEW NEXT INCIDENT".
- Failure: When the queue contains multiple items, the dequeue logic reads and erases index 1 instead of index 0, skipping INC-001 and violating FIFO arrival ordering.
- Expected: Dequeuing must take `pendingQueue_.front()`, ensuring pending incidents are reviewed strictly in arrival order.

6. Driver Penalty State Restoration on Decision Undo
- Bug location: `src/service/steward_decisions.cpp`, within `StewardDecisions::undoLatestDecision()`.
- How to observe it: Apply a 5-second time penalty to Car #1, observe total penalty seconds become 5, then call `POST /api/decisions/undo` or click "UNDO LATEST ACTION".
- Failure: The undo handler pops the decision from the stack and resets warning counters, but fails to restore `totalPenaltySeconds` from `top.prevTotalPenaltySeconds`.
- Expected: Undo must restore all snapshotted driver fields (`totalPenaltySeconds`, `warningsCount`, `isDisqualified`) and recalculate leaderboard standings accordingly.
