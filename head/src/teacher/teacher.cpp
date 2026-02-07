#include "teacher/teacher.h"

#include <iostream>

// TODO: #include <curl/curl.h>
// TODO: #include <nlohmann/json.hpp>

namespace prometheus {

struct Teacher::Impl {
    std::string api_base_url;
    std::string api_key;      // loaded from env GEMINI_API_KEY
    // CURL* curl_handle = nullptr;
};

Teacher::Teacher(const std::string& api_base_url)
    : impl_(std::make_unique<Impl>())
{
    impl_->api_base_url = api_base_url;

    // Read key from environment — never hard-coded.
    const char* key = std::getenv("GEMINI_API_KEY");
    if (key) impl_->api_key = key;
}

Teacher::~Teacher() = default;

std::string Teacher::grade_log(const std::string& log) {
    std::cout << "[TEACHER] Grading log (" << log.size() << " chars)...\n";

    if (impl_->api_key.empty()) {
        std::cerr << "[TEACHER] WARNING: GEMINI_API_KEY not set — skipping.\n";
        return "";
    }

    // ── Gemini API call (stubbed) ───────────────────────────────
    // nlohmann::json payload = {
    //     {"contents", {{
    //         {"parts", {{
    //             {"text", "You are the Teacher of Prometheus. "
    //                      "Grade the following daily log and produce "
    //                      "a single concise lesson.\n\n" + log}
    //         }}}
    //     }}}
    // };
    //
    // std::string url = impl_->api_base_url
    //     + "/v1beta/models/gemini-3-pro:generateContent"
    //     + "?key=" + impl_->api_key;
    // std::string resp = http_post(url, payload.dump());
    // auto j = nlohmann::json::parse(resp);
    // return j["candidates"][0]["content"]["parts"][0]["text"];

    std::cout << "[TEACHER] Grading complete (stub).\n";
    return "Be patient when approaching new biomes.";
}

std::string Teacher::request_calm() {
    // The "Walled Garden" — always accessible, even offline.
    return "You are special. You are unique. "
           "There is no one in the whole world like you. "
           "And people can like you exactly as you are. — Mr. Rogers";
}

} // namespace prometheus
