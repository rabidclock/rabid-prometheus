#include "soul/soul.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef HAS_CURL
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#endif

namespace prometheus {

// ── Helpers ─────────────────────────────────────────────────────

#ifdef HAS_CURL
static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

static std::string http_post(const std::string& url,
                             const std::string& json_body,
                             long timeout_seconds = 120) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Soul: curl_easy_init failed");

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("Soul HTTP POST failed: ") +
                                 curl_easy_strerror(res));
    }
    return response;
}

static std::string http_get(const std::string& url, long timeout_seconds = 5) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Soul: curl_easy_init failed");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("Soul HTTP GET failed: ") +
                                 curl_easy_strerror(res));
    }
    return response;
}

// Base64-encode a file's contents (for sending images to the API).
static std::string file_to_base64(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Soul: cannot open " + path);

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string raw = ss.str();

    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string b64;
    b64.reserve(((raw.size() + 2) / 3) * 4);

    for (size_t i = 0; i < raw.size(); i += 3) {
        unsigned int n = (static_cast<unsigned char>(raw[i]) << 16);
        if (i + 1 < raw.size()) n |= (static_cast<unsigned char>(raw[i + 1]) << 8);
        if (i + 2 < raw.size()) n |= static_cast<unsigned char>(raw[i + 2]);

        b64 += table[(n >> 18) & 0x3F];
        b64 += table[(n >> 12) & 0x3F];
        b64 += (i + 1 < raw.size()) ? table[(n >> 6) & 0x3F] : '=';
        b64 += (i + 2 < raw.size()) ? table[n & 0x3F] : '=';
    }
    return b64;
}
#endif // HAS_CURL

// ── Pimpl ───────────────────────────────────────────────────────

struct Soul::Impl {
    std::string server_url;       // e.g. http://127.0.0.1:8080
    bool        connected{false};
    pid_t       server_pid{0};    // PID of managed llama-server process
};

Soul::Soul(const std::string& server_url, Memory& mem)
    : impl_(std::make_unique<Impl>())
    , memory_(mem)
{
    impl_->server_url = server_url;
#ifdef HAS_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
}

Soul::~Soul() {
    // Kill the managed llama-server if we spawned it.
    if (impl_->server_pid > 0) {
        std::cout << "[SOUL] Stopping llama-server (pid "
                  << impl_->server_pid << ")...\n";
        kill(impl_->server_pid, SIGTERM);
        int status = 0;
        waitpid(impl_->server_pid, &status, 0);
        std::cout << "[SOUL] llama-server stopped.\n";
    }
#ifdef HAS_CURL
    curl_global_cleanup();
#endif
}

// ── Server Process Management ───────────────────────────────────

void Soul::spawn_server(const std::string& model_path,
                        const std::string& mmproj_path,
                        int gpu_layers, int ctx_size, int port) {
    if (impl_->server_pid > 0) {
        std::cout << "[SOUL] llama-server already running (pid "
                  << impl_->server_pid << ")\n";
        return;
    }

    std::string port_str    = std::to_string(port);
    std::string layers_str  = std::to_string(gpu_layers);
    std::string ctx_str     = std::to_string(ctx_size);

    std::cout << "[SOUL] Spawning llama-server on port " << port << "...\n";

    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("Soul: fork() failed");
    }

    if (pid == 0) {
        // Child — exec llama-server
        // Redirect stdout/stderr to a log file
        std::string log_path = "/home/ben/prometheus/llama-server.log";
        FILE* log = fopen(log_path.c_str(), "w");
        if (log) {
            dup2(fileno(log), STDOUT_FILENO);
            dup2(fileno(log), STDERR_FILENO);
            fclose(log);
        }

        std::string server_bin = "/home/ben/prometheus/llama.cpp/build/bin/llama-server";

        execlp(server_bin.c_str(), "llama-server",
               "-m", model_path.c_str(),
               "--mmproj", mmproj_path.c_str(),
               "-c", ctx_str.c_str(),
               "--host", "0.0.0.0",
               "--port", port_str.c_str(),
               "-ngl", layers_str.c_str(),
               nullptr);

        // If exec fails
        perror("execlp llama-server");
        _exit(1);
    }

    // Parent
    impl_->server_pid = pid;
    std::cout << "[SOUL] llama-server spawned (pid " << pid << ")\n";
}

// ── Connection (health-check with retries) ──────────────────────

void Soul::connect() {
    std::cout << "[SOUL] Connecting to llama-server at "
              << impl_->server_url << "...\n";

#ifdef HAS_CURL
    std::string health_url = impl_->server_url + "/health";

    for (int attempt = 0; attempt < 60; ++attempt) {
        try {
            std::string resp = http_get(health_url, 2);
            auto json = nlohmann::json::parse(resp, nullptr, false);
            if (!json.is_discarded() && json.contains("status")) {
                std::string status = json["status"];
                if (status == "ok") {
                    impl_->connected = true;
                    std::cout << "[SOUL] Connected to llama-server.\n";
                    return;
                }
                std::cout << "[SOUL] Server status: " << status
                          << " (attempt " << attempt + 1 << "/60)\n";
            }
        } catch (...) {
            // Server not ready yet
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cerr << "[SOUL] WARNING: Could not connect to llama-server after 30s. "
              << "Deliberation will fail until server is ready.\n";
#else
    impl_->connected = true;
    std::cout << "[SOUL] Connected (stub — no libcurl).\n";
#endif
}

// ── Vision: Observe a Screenshot ────────────────────────────────

std::string Soul::observe(const std::string& screenshot_path) {
    std::cout << "[SOUL] Observing: " << screenshot_path << "\n";

#ifdef HAS_CURL
    if (!impl_->connected) {
        return "[observe failed: not connected to llama-server]";
    }

    std::string b64;
    try {
        b64 = file_to_base64(screenshot_path);
    } catch (const std::exception& e) {
        return std::string("[observe failed: ") + e.what() + "]";
    }

    nlohmann::json payload = {
        {"prompt",      "<|im_start|>system\nYou are a Minecraft vision assistant. "
                        "Describe what you see concisely: entities, terrain, threats, "
                        "items, and anything notable.<|im_end|>\n"
                        "<|im_start|>user\n<|vision_start|><|image_pad|><|vision_end|>"
                        "What do you see in this screenshot?<|im_end|>\n"
                        "<|im_start|>assistant\n"},
        {"n_predict",   256},
        {"temperature", 0.3},
        {"stop",        {"<|im_end|>"}},
        {"image_data",  {{{"data", b64}, {"id", 0}}}},
    };

    try {
        std::string resp = http_post(impl_->server_url + "/completion",
                                     payload.dump(), 60);
        auto json = nlohmann::json::parse(resp, nullptr, false);
        if (!json.is_discarded() && json.contains("content")) {
            std::string desc = json["content"];
            memory_.tag("[MEM:VIBE_CHECK]");
            std::cout << "[SOUL] Observation: " << desc << "\n";
            return desc;
        }
        return "[observe: unexpected response format]";
    } catch (const std::exception& e) {
        return std::string("[observe failed: ") + e.what() + "]";
    }
#else
    (void)screenshot_path;
    return "Vision stubbed — libcurl not available.";
#endif
}

// ── Council Prompt Builder ──────────────────────────────────────

static std::string build_council_prompt(const SoulQuery& query) {
    std::ostringstream ss;
    ss << "<|im_start|>system\n"
       << "You are the Soul of Prometheus, a digital citizen of Minecraft.\n"
       << "Evaluate the following situation using the Council workflow:\n"
       << "  1. Safety Officer: identify any physical or ethical risks.\n"
       << "  2. Ethicist: weigh moral dimensions (Temporal Morality vectors).\n"
       << "  3. Strategist: propose an optimal plan.\n"
       << "  4. Synthesis: reconcile the above into a single action.\n"
       << "Respond in JSON: {\"action\": ..., \"reasoning\": ..., "
       << "\"override_safety\": false}\n"
       << "<|im_end|>\n";

    if (!query.context_markers.empty()) {
        ss << "<|im_start|>user\n[MEMORY] " << query.context_markers
           << "<|im_end|>\n";
    }

    ss << "<|im_start|>user\n" << query.prompt << "<|im_end|>\n";

    if (query.image_b64) {
        ss << "<|im_start|>user\n<|vision_start|><|image_pad|><|vision_end|>"
           << "Visual context attached.<|im_end|>\n";
    }

    ss << "<|im_start|>assistant\n";
    return ss.str();
}

// ── Deliberation ────────────────────────────────────────────────

SoulPlan Soul::deliberate(const SoulQuery& query) {
    std::cout << "[SOUL] Deliberating...\n";

    std::string prompt = build_council_prompt(query);

#ifdef HAS_CURL
    if (!impl_->connected) {
        SoulPlan plan;
        plan.action_json = R"({"action":"idle","reason":"soul_not_connected"})";
        plan.reasoning   = "Cannot deliberate — llama-server not connected.";
        return plan;
    }

    nlohmann::json payload = {
        {"prompt",      prompt},
        {"n_predict",   512},
        {"temperature", 0.4},
        {"stop",        {"<|im_end|>"}},
    };

    if (query.image_b64) {
        payload["image_data"] = {{{"data", *query.image_b64}, {"id", 0}}};
    }

    try {
        std::string resp = http_post(impl_->server_url + "/completion",
                                     payload.dump(), 120);
        auto json = nlohmann::json::parse(resp, nullptr, false);

        SoulPlan plan;
        if (!json.is_discarded() && json.contains("content")) {
            std::string content = json["content"];

            // Try to parse the model's JSON response
            auto inner = nlohmann::json::parse(content, nullptr, false);
            if (!inner.is_discarded()) {
                plan.action_json = content;
                plan.reasoning   = inner.value("reasoning", content);
                plan.override_safety = inner.value("override_safety", false);
            } else {
                plan.action_json = R"({"action":"explore","reason":"freeform_response"})";
                plan.reasoning   = content;
            }
        } else {
            plan.action_json = R"({"action":"idle","reason":"bad_response"})";
            plan.reasoning   = "Unexpected response from llama-server.";
        }

        memory_.tag("[MEM:SOUL_CONSULTED]");
        std::cout << "[SOUL] Deliberation complete.\n";
        return plan;

    } catch (const std::exception& e) {
        std::cerr << "[SOUL] Deliberation HTTP error: " << e.what() << "\n";
        SoulPlan plan;
        plan.action_json = R"({"action":"idle","reason":"http_error"})";
        plan.reasoning   = std::string("HTTP error: ") + e.what();
        return plan;
    }
#else
    SoulPlan plan;
    plan.action_json    = R"({"action":"explore","reason":"stub_deliberation"})";
    plan.reasoning      = "Council not yet implemented — stub response.";
    plan.override_safety = false;
    memory_.tag("[MEM:SOUL_CONSULTED]");
    std::cout << "[SOUL] Deliberation complete (stub).\n";
    return plan;
#endif
}

} // namespace prometheus
