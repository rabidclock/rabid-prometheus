#include "memory/memory.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace prometheus {

static constexpr size_t kMaxMarkers = 64;

void Memory::init() {
    std::cout << "[MEMORY] Initialised (short-term: in-process, "
                 "long-term: ChromaDB stub).\n";
}

void Memory::tag(const std::string& marker) {
    std::lock_guard lock(mu_);
    // Deduplicate consecutive identical markers.
    if (!markers_.empty() && markers_.back() == marker) return;

    markers_.push_back(marker);

    // Sliding window — drop oldest when limit exceeded.
    if (markers_.size() > kMaxMarkers) {
        markers_.erase(markers_.begin());
    }
}

std::string Memory::active_markers() const {
    std::lock_guard lock(mu_);
    std::ostringstream ss;
    for (size_t i = 0; i < markers_.size(); ++i) {
        if (i) ss << ' ';
        ss << markers_[i];
    }
    return ss.str();
}

std::vector<std::string> Memory::recall(const std::string& marker) const {
    (void)marker;
    // TODO: Query ChromaDB via HTTP.
    // curl POST http://localhost:8000/api/v1/collections/<id>/query
    // { "query_texts": [marker], "n_results": 3 }
    return {};
}

void Memory::flush_log(const std::string& log_text) {
    (void)log_text;
    // TODO: Write to disk + upsert into ChromaDB for long-term recall.
    std::cout << "[MEMORY] Log flushed (" << log_text.size() << " bytes).\n";
}

void Memory::clear_short_term() {
    std::lock_guard lock(mu_);
    markers_.clear();
    std::cout << "[MEMORY] Short-term markers cleared.\n";
}

} // namespace prometheus
