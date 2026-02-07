#include "ipc/body_link.h"

#include <iostream>
#include <string>
#include <regex>

#if HAS_ZMQ
#include <zmq.h>
#include <nlohmann/json.hpp>
#endif

namespace prometheus {

// ---------------------------------------------------------------------------
// Helper: derive PUSH endpoint from SUB endpoint (port + 1)
// ---------------------------------------------------------------------------
static std::string derive_push_endpoint(const std::string& sub_ep) {
    // Match trailing port number, e.g. "tcp://127.0.0.1:5555"
    std::regex re(R"(^(.*:)(\d+)$)");
    std::smatch m;
    if (std::regex_match(sub_ep, m, re)) {
        int port = std::stoi(m[2].str()) + 1;
        return m[1].str() + std::to_string(port);
    }
    // Fallback: just append a separate port
    return sub_ep + "1";
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct BodyLink::Impl {
    std::string sub_endpoint;
    std::string push_endpoint;
    bool        connected{false};
#if HAS_ZMQ
    void* zmq_ctx  = nullptr;
    void* zmq_sub  = nullptr;
    void* zmq_push = nullptr;
#endif
};

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------
BodyLink::BodyLink(const std::string& sub_endpoint, const std::string& push_endpoint)
    : impl_(std::make_unique<Impl>())
{
    impl_->sub_endpoint  = sub_endpoint;
    impl_->push_endpoint = push_endpoint;
}

BodyLink::BodyLink(const std::string& sub_endpoint)
    : BodyLink(sub_endpoint, derive_push_endpoint(sub_endpoint))
{
}

BodyLink::~BodyLink() { disconnect(); }

// ---------------------------------------------------------------------------
// connect / disconnect
// ---------------------------------------------------------------------------
void BodyLink::connect() {
    std::cout << "[BODY] Connecting SUB to " << impl_->sub_endpoint
              << ", PUSH to " << impl_->push_endpoint << "...\n";

#if HAS_ZMQ
    impl_->zmq_ctx = zmq_ctx_new();

    // SUB socket — subscribe to all percept messages
    impl_->zmq_sub = zmq_socket(impl_->zmq_ctx, ZMQ_SUB);
    zmq_connect(impl_->zmq_sub, impl_->sub_endpoint.c_str());
    zmq_setsockopt(impl_->zmq_sub, ZMQ_SUBSCRIBE, "", 0);

    // PUSH socket — send actions to the body
    impl_->zmq_push = zmq_socket(impl_->zmq_ctx, ZMQ_PUSH);
    zmq_connect(impl_->zmq_push, impl_->push_endpoint.c_str());

    impl_->connected = true;
    std::cout << "[BODY] Connected.\n";
#else
    impl_->connected = true;
    std::cout << "[BODY] Connected (stub — ZMQ not available).\n";
#endif
}

void BodyLink::disconnect() {
    if (!impl_ || !impl_->connected) return;

#if HAS_ZMQ
    if (impl_->zmq_sub)  zmq_close(impl_->zmq_sub);
    if (impl_->zmq_push) zmq_close(impl_->zmq_push);
    if (impl_->zmq_ctx)  zmq_ctx_destroy(impl_->zmq_ctx);
    impl_->zmq_sub  = nullptr;
    impl_->zmq_push = nullptr;
    impl_->zmq_ctx  = nullptr;
#endif

    impl_->connected = false;
    std::cout << "[BODY] Disconnected.\n";
}

// ---------------------------------------------------------------------------
// poll_percept — non-blocking receive on SUB
// ---------------------------------------------------------------------------
std::optional<Percept> BodyLink::poll_percept() {
    if (!impl_->connected) return std::nullopt;

#if HAS_ZMQ
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int rc = zmq_msg_recv(&msg, impl_->zmq_sub, ZMQ_DONTWAIT);
    if (rc == -1) {
        zmq_msg_close(&msg);
        return std::nullopt;
    }

    std::string data(static_cast<char*>(zmq_msg_data(&msg)),
                     zmq_msg_size(&msg));
    zmq_msg_close(&msg);

    // Parse JSON into Percept
    try {
        auto j = nlohmann::json::parse(data);

        Percept p;
        p.raw_json       = data;
        p.health         = j.value("health", 20.0f);
        p.hunger         = j.value("food", 20.0f);
        p.hostile_nearby = false;

        if (j.contains("nearby_entities") && j["nearby_entities"].is_array()) {
            for (auto& ent : j["nearby_entities"]) {
                if (ent.value("hostile", false)) {
                    p.hostile_nearby = true;
                    break;
                }
            }
        }

        return p;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[BODY] JSON parse error: " << e.what() << "\n";
        return std::nullopt;
    }
#else
    return std::nullopt; // stub
#endif
}

// ---------------------------------------------------------------------------
// send_action
// ---------------------------------------------------------------------------
void BodyLink::send_action(const std::string& action_json) {
    if (!impl_->connected) return;

#if HAS_ZMQ
    zmq_send(impl_->zmq_push, action_json.data(),
             action_json.size(), ZMQ_DONTWAIT);
#endif

    std::cout << "[BODY] >> " << action_json << "\n";
}

} // namespace prometheus
