#include <iostream>
#include <string>
#include <cstdlib>
#include <memory>
#include "httplib.h"
#include "json.hpp"
#include "service/timing_engine.hpp"
#include "service/radio_parser.hpp"
#include "service/pit_strategy.hpp"
#include "service/incident_queue.hpp"
#include "service/steward_decisions.hpp"

using json = nlohmann::json;
using namespace apex;

int main() {
    TimingEngine timingEngine;
    RadioParser radioParser;
    PitStrategy pitStrategy;
    IncidentQueue incidentQueue;
    StewardDecisions stewardDecisions;

    // Initialize steward baseline
    stewardDecisions.initializeDefaultDecisions(timingEngine);

    httplib::Server svr;

    // Enable CORS and common headers
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"},
        {"Cache-Control", "no-cache, no-store, must-revalidate"}
    });

    svr.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
    });

    // Health check
    svr.Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        json j = {
            {"status", "ok"},
            {"service", "ApexRace Control Server"},
            {"version", "1.0.0"}
        };
        res.set_content(j.dump(), "application/json");
    });

    // Full Race State
    svr.Get("/api/state", [&](const httplib::Request&, httplib::Response& res) {
        json j = {
            {"session", timingEngine.getSessionState()},
            {"drivers", timingEngine.getDrivers()},
            {"radio", radioParser.getAllMessages()},
            {"pitStrategy", pitStrategy.getOrderedStops()},
            {"incidents", {
                {"pending", incidentQueue.getPendingIncidents()},
                {"resolved", incidentQueue.getResolvedIncidents()}
            }},
            {"decisions", stewardDecisions.getDecisionHistory()},
            {"undoStackDepth", stewardDecisions.getUndoStackDepth()}
        };
        res.set_content(j.dump(), "application/json");
    });

    // Timing Tower
    svr.Get("/api/timing", [&](const httplib::Request&, httplib::Response& res) {
        json j = {
            {"session", timingEngine.getSessionState()},
            {"drivers", timingEngine.getDrivers()}
        };
        res.set_content(j.dump(), "application/json");
    });

    // Single Driver details
    svr.Get(R"(/api/drivers/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string driverId = req.matches[1];
        const Driver* d = timingEngine.getDriver(driverId);
        if (!d) {
            res.status = 404;
            json err = {{"error", "Driver not found"}, {"id", driverId}};
            res.set_content(err.dump(), "application/json");
            return;
        }
        json j = *d;
        res.set_content(j.dump(), "application/json");
    });

    // Record lap
    svr.Post("/api/laps", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string driverId = body.value("driverId", "");
            double lapTime = body.value("lapTime", 0.0);
            double s1 = body.value("s1", 0.0);
            double s2 = body.value("s2", 0.0);
            double s3 = body.value("s3", 0.0);

            if (driverId.empty() || lapTime <= 0.0) {
                res.status = 400;
                json err = {{"error", "Invalid driverId or lapTime"}};
                res.set_content(err.dump(), "application/json");
                return;
            }

            bool ok = timingEngine.recordLap(driverId, lapTime, s1, s2, s3);
            if (!ok) {
                res.status = 404;
                json err = {{"error", "Driver not found"}};
                res.set_content(err.dump(), "application/json");
                return;
            }

            res.status = 200;
            json resp = {{"success", true}, {"driver", *timingEngine.getDriver(driverId)}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", "JSON parsing failed"}, {"details", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // Radio messages
    svr.Get("/api/radio", [&](const httplib::Request& req, httplib::Response& res) {
        std::string query = req.get_param_value("query");
        std::string category = req.get_param_value("category");
        std::string carStr = req.get_param_value("carNumber");
        int carNumber = carStr.empty() ? 0 : std::atoi(carStr.c_str());

        std::vector<RadioMessage> filtered = radioParser.filterMessages(query, category, carNumber);
        json j = filtered;
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/radio", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            if (body.contains("flagAction")) {
                std::string msgId = body.value("messageId", "");
                bool flagged = body.value("flagged", true);
                bool ok = radioParser.flagMessage(msgId, flagged);
                json resp = {{"success", ok}, {"messageId", msgId}, {"flagged", flagged}};
                res.set_content(resp.dump(), "application/json");
                return;
            }

            RadioMessage msg;
            msg.id = "rad_" + std::to_string(radioParser.getAllMessages().size() + 101);
            msg.sender = body.value("sender", "Pit Wall");
            msg.carNumber = body.value("carNumber", 0);
            msg.category = body.value("category", "GENERAL");
            msg.text = body.value("text", "");
            msg.timestamp = body.value("timestamp", "14:24:00");
            msg.isFlagged = body.value("isFlagged", false);

            if (msg.text.empty()) {
                res.status = 400;
                json err = {{"error", "Message text cannot be empty"}};
                res.set_content(err.dump(), "application/json");
                return;
            }

            radioParser.addMessage(msg);
            res.status = 201;
            json resp = {{"success", true}, {"message", msg}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", "JSON parsing failed"}, {"details", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // Pit Strategy Board (Linked List)
    svr.Get("/api/pit-strategy", [&](const httplib::Request&, httplib::Response& res) {
        json j = {
            {"stops", pitStrategy.getOrderedStops()},
            {"count", pitStrategy.getStopCount()}
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/pit-strategy/stops", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            PitStop stop;
            stop.id = body.value("id", "");
            stop.carNumber = body.value("carNumber", 0);
            stop.plannedLap = body.value("plannedLap", 0);
            stop.stopType = body.value("stopType", "MEDIUM");
            stop.status = body.value("status", "SCHEDULED");

            auto result = pitStrategy.addStop(stop);
            if (!result.first) {
                res.status = 400;
                json err = {{"error", result.second}};
                res.set_content(err.dump(), "application/json");
                return;
            }

            res.status = 201;
            json resp = {
                {"success", true},
                {"id", result.second},
                {"stops", pitStrategy.getOrderedStops()}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", "Invalid JSON"}, {"details", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Put(R"(/api/pit-strategy/stops/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string stopId = req.matches[1];
            auto body = json::parse(req.body);
            std::string action = body.value("action", "");

            if (action == "move_earlier") {
                bool ok = pitStrategy.moveStopEarlier(stopId);
                json resp = {{"success", ok}, {"stops", pitStrategy.getOrderedStops()}};
                res.set_content(resp.dump(), "application/json");
                return;
            } else if (action == "move_later") {
                bool ok = pitStrategy.moveStopLater(stopId);
                json resp = {{"success", ok}, {"stops", pitStrategy.getOrderedStops()}};
                res.set_content(resp.dump(), "application/json");
                return;
            } else if (body.contains("status")) {
                std::string status = body.value("status", "SCHEDULED");
                bool ok = pitStrategy.updateStopStatus(stopId, status);
                json resp = {{"success", ok}, {"stops", pitStrategy.getOrderedStops()}};
                res.set_content(resp.dump(), "application/json");
                return;
            }

            res.status = 400;
            json err = {{"error", "Unsupported action or missing status"}};
            res.set_content(err.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", "Invalid JSON"}, {"details", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Delete(R"(/api/pit-strategy/stops/([^/]+))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string stopId = req.matches[1];
        bool ok = pitStrategy.removeStop(stopId);
        if (!ok) {
            res.status = 404;
            json err = {{"error", "Stop not found"}};
            res.set_content(err.dump(), "application/json");
            return;
        }
        json resp = {{"success", true}, {"stops", pitStrategy.getOrderedStops()}};
        res.set_content(resp.dump(), "application/json");
    });

    // Incident Control (FIFO Queue)
    svr.Get("/api/incidents", [&](const httplib::Request&, httplib::Response& res) {
        const Incident* nextInc = incidentQueue.peekNextIncident();
        json j = {
            {"pending", incidentQueue.getPendingIncidents()},
            {"resolved", incidentQueue.getResolvedIncidents()},
            {"pendingCount", incidentQueue.getPendingCount()},
            {"nextIncident", (nextInc ? json(*nextInc) : json(nullptr))}
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/incidents", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int lap = body.value("lap", timingEngine.getSessionState().currentLap);
            std::string location = body.value("location", "Sector 1");
            std::vector<int> involvedCars;
            if (body.contains("involvedCars") && body["involvedCars"].is_array()) {
                involvedCars = body["involvedCars"].get<std::vector<int>>();
            }
            std::string category = body.value("category", "COLLISION");
            std::string severity = body.value("severity", "MEDIUM");
            std::string description = body.value("description", "");

            if (description.empty()) {
                res.status = 400;
                json err = {{"error", "Description cannot be empty"}};
                res.set_content(err.dump(), "application/json");
                return;
            }

            Incident inc = incidentQueue.reportIncident(lap, location, involvedCars, category, severity, description);
            timingEngine.addSessionLog("New Incident reported: " + inc.id + " at " + inc.location);

            res.status = 201;
            json resp = {{"success", true}, {"incident", inc}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", "Invalid JSON"}, {"details", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Post("/api/incidents/process-next", [&](const httplib::Request&, httplib::Response& res) {
        auto result = incidentQueue.processNextIncident();
        if (!result.first) {
            res.status = 404;
            json err = {{"error", "No pending incidents in queue"}};
            res.set_content(err.dump(), "application/json");
            return;
        }

        timingEngine.addSessionLog("Incident [" + result.second.id + "] pulled for investigation");
        json resp = {
            {"success", true},
            {"selectedIncident", result.second},
            {"pendingCount", incidentQueue.getPendingCount()}
        };
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post(R"(/api/incidents/([^/]+)/resolve)", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string incidentId = req.matches[1];
            auto body = json::parse(req.body);
            std::string status = body.value("status", "RESOLVED");
            std::string notes = body.value("reviewNotes", "Resolved by race director");

            bool ok = incidentQueue.resolveIncident(incidentId, status, notes);
            if (!ok) {
                res.status = 404;
                json err = {{"error", "Incident not found in pending queue"}};
                res.set_content(err.dump(), "application/json");
                return;
            }

            timingEngine.addSessionLog("Incident [" + incidentId + "] marked " + status);
            json resp = {{"success", true}, {"incidentId", incidentId}, {"status", status}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", "Invalid JSON"}, {"details", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    // Steward Decisions (Stack-based Undo)
    svr.Get("/api/decisions", [&](const httplib::Request&, httplib::Response& res) {
        json j = {
            {"decisions", stewardDecisions.getDecisionHistory()},
            {"undoStackDepth", stewardDecisions.getUndoStackDepth()}
        };
        res.set_content(j.dump(), "application/json");
    });

    svr.Post("/api/decisions", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            StewardDecision decision;
            decision.incidentId = body.value("incidentId", "");
            decision.carNumber = body.value("carNumber", 0);
            decision.driverId = body.value("driverId", "");
            decision.actionType = body.value("actionType", "WARNING");
            decision.penaltySeconds = body.value("penaltySeconds", 0);
            decision.reason = body.value("reason", "Race Control Directive");
            decision.timestamp = body.value("timestamp", "14:25:00");
            decision.reversible = body.value("reversible", true);

            auto result = stewardDecisions.applyDecision(decision, timingEngine);
            if (!result.first) {
                res.status = 400;
                json err = {{"error", result.second}};
                res.set_content(err.dump(), "application/json");
                return;
            }

            res.status = 201;
            json resp = {
                {"success", true},
                {"decisionId", result.second},
                {"decisions", stewardDecisions.getDecisionHistory()},
                {"drivers", timingEngine.getDrivers()}
            };
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", "Invalid JSON"}, {"details", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Post("/api/decisions/undo", [&](const httplib::Request&, httplib::Response& res) {
        auto result = stewardDecisions.undoLatestDecision(timingEngine);
        if (!result.first) {
            res.status = 400;
            json err = {{"error", "No reversible actions on the undo stack"}};
            res.set_content(err.dump(), "application/json");
            return;
        }

        json resp = {
            {"success", true},
            {"undoneDecision", result.second},
            {"decisions", stewardDecisions.getDecisionHistory()},
            {"undoStackDepth", stewardDecisions.getUndoStackDepth()},
            {"drivers", timingEngine.getDrivers()}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // Session Control
    svr.Post("/api/session/advance", [&](const httplib::Request&, httplib::Response& res) {
        timingEngine.advanceSessionLap();
        json resp = {
            {"success", true},
            {"session", timingEngine.getSessionState()},
            {"drivers", timingEngine.getDrivers()}
        };
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post("/api/session/status", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            if (body.contains("trackStatus")) {
                std::string status = body.value("trackStatus", "GREEN");
                timingEngine.setTrackStatus(status);
            }
            if (body.contains("running")) {
                bool running = body.value("running", false);
                timingEngine.setSimRunning(running);
            }
            json resp = {{"success", true}, {"session", timingEngine.getSessionState()}};
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err = {{"error", "Invalid JSON"}, {"details", e.what()}};
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Post("/api/reset", [&](const httplib::Request&, httplib::Response& res) {
        timingEngine.reset();
        radioParser.reset();
        pitStrategy.reset();
        incidentQueue.reset();
        stewardDecisions.reset();
        stewardDecisions.initializeDefaultDecisions(timingEngine);

        json resp = {
            {"success", true},
            {"message", "ApexRace session reset to default seeded state"},
            {"session", timingEngine.getSessionState()},
            {"drivers", timingEngine.getDrivers()}
        };
        res.set_content(resp.dump(), "application/json");
    });

    // Static files mount
    svr.set_mount_point("/", "./public");

    // Port resolution
    int port = 8080;
    const char* envPort = std::getenv("PORT");
    if (envPort && std::string(envPort).length() > 0) {
        port = std::atoi(envPort);
    }

    std::cout << "[ApexRace] Motorsport Race Control System starting on 0.0.0.0:" << port << std::endl;
    if (!svr.listen("0.0.0.0", port)) {
        std::cerr << "[ApexRace Error] Failed to bind to 0.0.0.0:" << port << std::endl;
        return 1;
    }

    return 0;
}
