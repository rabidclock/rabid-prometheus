#pragma once

#include "memory/memory.h"

#include <memory>
#include <optional>
#include <string>

namespace prometheus {

// A symbolic percept streamed from the mineflayer body (JSON-decoded).
struct Percept {
    std::string raw_json;           // full JSON blob
    bool        hostile_nearby{};   // quick-check flag
    float       health{20.0f};
    float       hunger{20.0f};
};

// A reflex command produced by the Lizard.
struct Reflex {
    enum class Layer : uint8_t {
        Avoid  = 0,   // Layer 0 — flee / dodge
        Tactic = 1,   // Layer 1 — pathfind / eat
    };

    Layer       layer;
    std::string action_json;        // serialised intent for the body
    float       urgency{0.0f};      // 0.0 = low … 1.0 = critical
    bool        vetoes_soul{};      // true → override any Soul plan
};

// System 1 — The Lizard Brain
// Wraps an embedded Phi-3.5-mini via libllama for < 100 ms reflexes.
class Lizard {
public:
    explicit Lizard(Memory& mem);
    ~Lizard();

    Lizard(const Lizard&) = delete;
    Lizard& operator=(const Lizard&) = delete;

    // Load the Phi-3.5 GGUF into VRAM/RAM.
    void load_model(const std::string& model_path = "models/phi-3.5-mini.gguf");

    // Produce a reflex from a symbolic percept.
    Reflex react(const Percept& percept);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Memory& memory_;
};

} // namespace prometheus
