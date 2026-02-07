#include "circadian/circadian.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace prometheus {

Circadian::Circadian(Memory& mem, Teacher& teacher)
    : memory_(mem), teacher_(teacher) {}

void Circadian::run(std::atomic<bool>& running) {
    using clock = std::chrono::steady_clock;

    std::cout << "[CIRCADIAN] Cycle started.\n";
    auto phase_start = clock::now();

    while (running.load()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            clock::now() - phase_start).count();

        switch (state_.load()) {
        case State::Awake:
            if (elapsed >= awake_duration_s_) {
                std::cout << "[CIRCADIAN] Transitioning to Tired.\n";
                state_.store(State::Tired);
                phase_start = clock::now();
            }
            break;

        case State::Tired:
            if (elapsed >= tired_duration_s_) {
                state_.store(State::Sleeping);
                enter_sleep();
                phase_start = clock::now();
            }
            break;

        case State::Sleeping:
            // enter_sleep() is synchronous; once it returns we wake.
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Circadian::enter_sleep() {
    std::cout << "[CIRCADIAN] Entering Sleep — consolidating...\n";

    // 1. Dump the day's log from memory.
    std::string log = memory_.active_markers();
    memory_.flush_log(log);

    // 2. Send to the Teacher (Gemini API) for grading.
    std::string lesson = teacher_.grade_log(log);

    // 3. Wake up and inject the lesson.
    wake_up(lesson);
}

void Circadian::wake_up(const std::string& lesson) {
    memory_.clear_short_term();

    if (!lesson.empty()) {
        memory_.tag("[MEM:LESSON:" + lesson.substr(0, 40) + "]");
        std::cout << "[CIRCADIAN] Lesson absorbed.\n";
    }

    state_.store(State::Awake);
    std::cout << "[CIRCADIAN] Good morning.\n";
}

} // namespace prometheus
