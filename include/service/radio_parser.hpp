#pragma once

#include <vector>
#include <string>
#include "model/race_models.hpp"

namespace apex {

class RadioParser {
public:
    RadioParser();

    void initializeDefaultFeed();
    void reset();

    void addMessage(const RadioMessage& msg);
    const std::vector<RadioMessage>& getAllMessages() const { return messages_; }
    const RadioMessage* getMessage(const std::string& id) const;

    std::vector<RadioMessage> filterMessages(const std::string& query,
                                             const std::string& category,
                                             int carNumber) const;

    bool flagMessage(const std::string& messageId, bool flagged);

    static std::string normalizeString(const std::string& input);
    static bool matchesKeyword(const std::string& haystack, const std::string& needle);

private:
    std::vector<RadioMessage> messages_;
};

} // namespace apex
