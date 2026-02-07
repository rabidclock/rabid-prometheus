#pragma once

#include "memory/memory.h"
#include "teacher/teacher.h"

#include <atomic>

namespace prometheus {

// Circadian Rhythm / Sleep Cycle state machine.
//   Awake → Tired → Sleep (consolidation) → Awake
class Circadian {
public:
    enum class State : uint8_t {
        Awake,
        Tired,
        Sleeping,
    };

    Circadian(Memory& mem, Teacher& teacher);

    // Blocking run-loop — call from a dedicated thread.
    void run(std::atomic<bool>& running);

    State state() const { return state_.load(); }

private:
    void enter_sleep();
    void wake_up(const std::string& lesson);

    Memory&  memory_;
    Teacher& teacher_;
    std::atomic<State> state_{State::Awake};

    // Configurable cycle length (seconds of game-time).
    int awake_duration_s_  = 1200;  // 20 min
    int tired_duration_s_  = 120;   //  2 min
};

} // namespace prometheus
