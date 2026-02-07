#pragma once

#include "memory/memory.h"

#include <memory>
#include <optional>
#include <string>

namespace prometheus {

// A high-level query escalated from the Arbiter to the Soul.
struct SoulQuery {
    std::string prompt;                     // assembled prompt text
    std::optional<std::string> image_b64;   // screenshot for Qwen-VL vision
    std::string context_markers;            // active memory markers
};

// The Soul's response — a deliberated plan or ethical assessment.
struct SoulPlan {
    std::string action_json;    // high-level intent for the body
    std::string reasoning;      // chain-of-thought / council trace
    bool        override_safety{false}; // [OVERRIDE: IGNORE_SAFETY] flag
};

// System 2 — The Soul
// Communicates with an external llama-server hosting Qwen 2.5-VL-32B
// over async HTTP. Handles vision tokens and the Council workflow.
class Soul {
public:
    Soul(const std::string& server_url, Memory& mem);
    ~Soul();

    Soul(const Soul&) = delete;
    Soul& operator=(const Soul&) = delete;

    // Verify the llama-server is reachable.
    void connect();

    // Synchronous deliberation (called from the soul thread).
    SoulPlan deliberate(const SoulQuery& query);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Memory& memory_;
};

} // namespace prometheus
