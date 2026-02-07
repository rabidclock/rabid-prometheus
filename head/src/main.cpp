#include "lizard/lizard.h"
#include "soul/soul.h"
#include "arbiter/arbiter.h"
#include "ipc/body_link.h"
#include "memory/memory.h"
#include "circadian/circadian.h"
#include "teacher/teacher.h"

#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

static std::atomic<bool> g_running{true};

static void signal_handler(int) { g_running.store(false); }

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "[HEAD] Prometheus Backplane v0.1.0 starting...\n";

    // ── Subsystems ──────────────────────────────────────────────
    prometheus::Memory    memory;
    prometheus::BodyLink  body("tcp://127.0.0.1:5555");
    prometheus::Lizard    lizard(memory);
    prometheus::Soul      soul("http://127.0.0.1:8080", memory);
    prometheus::Arbiter   arbiter(lizard, soul, body);
    prometheus::Teacher   teacher("https://generativelanguage.googleapis.com");
    prometheus::Circadian circadian(memory, teacher);

    // ── Boot ────────────────────────────────────────────────────
    memory.init();
    body.connect();
    lizard.load_model();
    soul.connect();

    std::cout << "[HEAD] All subsystems initialised.\n";

    // ── Threads ─────────────────────────────────────────────────
    // Lizard: tight reflex loop (targets < 100 ms per tick)
    std::thread lizard_thread([&] {
        while (g_running.load()) {
            auto percept = body.poll_percept();
            if (!percept) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            auto reflex = lizard.react(*percept);
            arbiter.submit_reflex(std::move(reflex));
        }
    });

    // Soul: deliberative loop (2–5 s per query)
    std::thread soul_thread([&] {
        while (g_running.load()) {
            auto query = arbiter.next_soul_query();
            if (!query) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            auto plan = soul.deliberate(*query);
            arbiter.submit_plan(std::move(plan));
        }
    });

    // Circadian: background clock
    std::thread circadian_thread([&] {
        circadian.run(g_running);
    });

    // ── Main loop: arbiter dispatches winning actions ───────────
    while (g_running.load()) {
        arbiter.dispatch_tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 Hz
    }

    // ── Shutdown ────────────────────────────────────────────────
    std::cout << "[HEAD] Shutting down...\n";
    g_running.store(false);

    lizard_thread.join();
    soul_thread.join();
    circadian_thread.join();

    body.disconnect();
    std::cout << "[HEAD] Goodbye.\n";
    return 0;
}
