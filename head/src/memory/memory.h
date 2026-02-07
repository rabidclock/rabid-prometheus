#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace prometheus {

// Memory subsystem — two tiers:
//   Short-term: in-context "Memory Markers" (e.g. [MEM:LAVA_DEATH]).
//   Long-term:  vector DB (ChromaDB) for full-text retrieval on marker hit.
class Memory {
public:
    void init();

    // Add a short-term marker to the rolling context.
    void tag(const std::string& marker);

    // Return all active markers as a single string for prompt injection.
    std::string active_markers() const;

    // Search the long-term store for entries matching a marker.
    // Returns full text passages (ChromaDB query, stubbed).
    std::vector<std::string> recall(const std::string& marker) const;

    // Persist the current session log (called during Sleep Cycle).
    void flush_log(const std::string& log_text);

    // Clear short-term markers (called on wake).
    void clear_short_term();

private:
    mutable std::mutex mu_;
    std::vector<std::string> markers_;
};

} // namespace prometheus
