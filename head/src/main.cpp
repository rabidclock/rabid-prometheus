#include "lizard/lizard.h"
#include "soul/soul.h"
#include "arbiter/arbiter.h"
#include "ipc/body_link.h"
#include "memory/memory.h"
#include "circadian/circadian.h"
#include "teacher/teacher.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

static std::atomic<bool> g_running{true};

static void signal_handler(int) { g_running.store(false); }

// Take a screenshot of the bot's prismarine-viewer via playwright.
// Falls back to a fixed path if the screenshot command fails.
static std::string take_screenshot() {
    std::string path = "/tmp/prometheus_vibe.png";
    // Use playwright (via venv) to capture the prismarine-viewer on port 3007
    (void)std::system(
        ("/home/ben/rabid-ui/venv/bin/python3 /home/ben/prometheus/head/scripts/capture_view.py " + path + " 2>/dev/null").c_str());
    return path;
}

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

    // Spawn llama-server and wait for it to be ready
    soul.spawn_server(
        "/home/ben/prometheus/models/Qwen2.5-VL-32B-Instruct-Q8_0.gguf",
        "/home/ben/prometheus/models/mmproj-Qwen2.5-VL-32B-Instruct-Q8_0.gguf",
        99,    // gpu layers
        4096,  // context size
        8080   // port
    );
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

    // Vibe Check: take a screenshot every 10 seconds and observe via Soul
    std::thread vibe_thread([&] {
        // Wait a bit for everything to settle
        std::this_thread::sleep_for(std::chrono::seconds(5));

        while (g_running.load()) {
            std::string screenshot = take_screenshot();
            std::string description = soul.observe(screenshot);
            std::cout << "[VIBE CHECK] " << description << "\n";

            // Also escalate to the soul for deeper reasoning
            arbiter.escalate("Vibe check — describe the current situation and suggest "
                             "what we should do next.",
                             soul.observe(screenshot));

            // Sleep 10 seconds (in 100ms increments so we can exit promptly)
            for (int i = 0; i < 100 && g_running.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
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
    vibe_thread.join();

    body.disconnect();
    std::cout << "[HEAD] Goodbye.\n";
    return 0;
}
