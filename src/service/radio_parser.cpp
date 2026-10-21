#include "service/radio_parser.hpp"
#include <algorithm>
#include <cctype>

namespace apex {

RadioParser::RadioParser() {
    initializeDefaultFeed();
}

void RadioParser::initializeDefaultFeed() {
    messages_.clear();

    messages_.push_back({
        "rad_001", "Red Bull Pit Wall", 1, "14:02:18", "STRATEGY",
        "Box this lap, box for hard tires. Traffic is clear on exit.", false
    });
    messages_.push_back({
        "rad_002", "NOR (#4)", 4, "14:05:44", "TECHNICAL",
        "Front left tire has heavy graining, losing rotation in Sector 2.", false
    });
    messages_.push_back({
        "rad_003", "Race Control", 0, "14:09:12", "INCIDENT",
        "Yellow flag Sector 2. Car 55 and Car 63 contact under review.", true
    });
    messages_.push_back({
        "rad_004", "Ferrari Pit Wall", 55, "14:10:05", "STRATEGY",
        "Check front wing endplate. Downforce numbers look stable on telemetry.", false
    });
    messages_.push_back({
        "rad_005", "Race Control", 0, "14:13:30", "STEWARDS",
        "Car 1 under investigation for track limits Turn 9.", true
    });
    messages_.push_back({
        "rad_006", "HAM (#44)", 44, "14:16:50", "TECHNICAL",
        "Engine temperature is climbing, need cooler air in high speed corners.", false
    });
    messages_.push_back({
        "rad_007", "Mercedes Pit Wall", 44, "14:17:15", "STRATEGY",
        "Copy Lewis, lift and coast into Turn 15 to manage thermal levels.", false
    });
    messages_.push_back({
        "rad_008", "Race Control", 0, "14:20:02", "INCIDENT",
        "Debris on main straight near pit exit. Marshals attending.", true
    });
}

void RadioParser::reset() {
    initializeDefaultFeed();
}

void RadioParser::addMessage(const RadioMessage& msg) {
    messages_.push_back(msg);
}

const RadioMessage* RadioParser::getMessage(const std::string& id) const {
    for (const auto& m : messages_) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

bool RadioParser::flagMessage(const std::string& messageId, bool flagged) {
    for (auto& m : messages_) {
        if (m.id == messageId) {
            m.isFlagged = flagged;
            return true;
        }
    }
    return false;
}

std::string RadioParser::normalizeString(const std::string& input) {
    size_t first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = input.find_last_not_of(" \t\r\n");
    std::string trimmed = input.substr(first, (last - first + 1));

#ifdef SEED_BUG_2
    // Bug 2 — String Processing:
    // Defect in case-insensitive normalization: only lowercases the first character!
    // Subsequent uppercase characters are left unmodified, causing case-insensitive
    // searches with multi-character or fully uppercase search terms (e.g. "BOX" or "DEBRIS")
    // to fail to match target messages.
    if (!trimmed.empty()) {
        trimmed[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[0])));
    }
    return trimmed;
#else
    // Correct behavior: all characters are converted to lowercase
    for (char& c : trimmed) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return trimmed;
#endif
}

bool RadioParser::matchesKeyword(const std::string& haystack, const std::string& needle) {
    std::string normHaystack = normalizeString(haystack);
    std::string normNeedle = normalizeString(needle);

    if (normNeedle.empty()) return true;
    return (normHaystack.find(normNeedle) != std::string::npos);
}

std::vector<RadioMessage> RadioParser::filterMessages(const std::string& query,
                                                     const std::string& category,
                                                     int carNumber) const {
    std::vector<RadioMessage> result;

    for (const auto& m : messages_) {
        if (!category.empty() && category != "ALL" && m.category != category) {
            continue;
        }

        if (carNumber > 0 && m.carNumber != carNumber) {
            continue;
        }

        if (!query.empty()) {
            bool textMatches = matchesKeyword(m.text, query);
            bool senderMatches = matchesKeyword(m.sender, query);
            if (!textMatches && !senderMatches) {
                continue;
            }
        }

        result.push_back(m);
    }

    return result;
}

} // namespace apex
