#include "soul/soul.h"

#include <iostream>
#include <stdexcept>
#include <sstream>

// TODO: #include <curl/curl.h>
// TODO: #include <nlohmann/json.hpp>

namespace prometheus {

// ── Pimpl ───────────────────────────────────────────────────────
struct Soul::Impl {
    std::string server_url;       // e.g. http://127.0.0.1:8080
    bool        connected{false};
    // CURL*    curl_handle = nullptr;
};

Soul::Soul(const std::string& server_url, Memory& mem)
    : impl_(std::make_unique<Impl>())
    , memory_(mem)
{
    impl_->server_url = server_url;
}

Soul::~Soul() {
    // if (impl_->curl_handle) curl_easy_cleanup(impl_->curl_handle);
}

void Soul::connect() {
    std::cout << "[SOUL] Connecting to llama-server at "
              << impl_->server_url << "...\n";

    // ── Health-check the llama-server (stubbed) ─────────────────
    // CURL* curl = curl_easy_init();
    // std::string url = impl_->server_url + "/health";
    // curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    // CURLcode res = curl_easy_perform(curl);
    // if (res != CURLE_OK)
    //     throw std::runtime_error("Soul: cannot reach llama-server");
    // curl_easy_cleanup(curl);

    impl_->connected = true;
    std::cout << "[SOUL] Connected (stub).\n";
}

// ── Council Prompt Builder ──────────────────────────────────────
// Implements the "Persona Chain" council inside a single generation.
static std::string build_council_prompt(const SoulQuery& query) {
    std::ostringstream ss;
    ss << "<|system|>\n"
       << "You are the Soul of Prometheus, a digital citizen.\n"
       << "Evaluate the following situation using the Council workflow:\n"
       << "  1. Safety Officer: identify any physical or ethical risks.\n"
       << "  2. Ethicist: weigh moral dimensions (Temporal Morality vectors).\n"
       << "  3. Strategist: propose an optimal plan.\n"
       << "  4. Synthesis: reconcile the above into a single action.\n"
       << "Respond in JSON: {\"action\": ..., \"reasoning\": ..., "
       << "\"override_safety\": false}\n"
       << "<|end|>\n";

    // Inject memory context
    if (!query.context_markers.empty()) {
        ss << "<|user|>\n[MEMORY] " << query.context_markers << "\n<|end|>\n";
    }

    ss << "<|user|>\n" << query.prompt << "\n<|end|>\n";

    // If a screenshot is provided, add the vision placeholder.
    if (query.image_b64) {
        ss << "<|image|>" << *query.image_b64 << "<|end|>\n";
    }

    ss << "<|assistant|>\n";
    return ss.str();
}

SoulPlan Soul::deliberate(const SoulQuery& query) {
    std::cout << "[SOUL] Deliberating...\n";

    std::string prompt = build_council_prompt(query);

    // ── HTTP POST to llama-server /completion (stubbed) ─────────
    // nlohmann::json payload = {
    //     {"prompt",      prompt},
    //     {"n_predict",   512},
    //     {"temperature", 0.4},
    //     {"stop",        {"<|end|>"}},
    // };
    // if (query.image_b64)
    //     payload["image_data"] = {{{"data", *query.image_b64}, {"id", 0}}};
    //
    // std::string response = http_post(
    //     impl_->server_url + "/completion", payload.dump());
    // auto json_resp = nlohmann::json::parse(response);
    // std::string content = json_resp["content"];

    SoulPlan plan;
    plan.action_json    = R"({"action":"explore","reason":"stub_deliberation"})";
    plan.reasoning      = "Council not yet implemented — stub response.";
    plan.override_safety = false;

    // Record that the Soul was consulted.
    memory_.tag("[MEM:SOUL_CONSULTED]");

    std::cout << "[SOUL] Deliberation complete (stub).\n";
    return plan;
}

} // namespace prometheus
