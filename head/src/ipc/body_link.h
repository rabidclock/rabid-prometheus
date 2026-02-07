#pragma once

#include "lizard/lizard.h"   // for Percept

#include <memory>
#include <optional>
#include <string>

namespace prometheus {

// ZeroMQ link to the mineflayer Node.js bot ("Body").
// Uses a PUB/SUB + PUSH/PULL pattern:
//   SUB: receives a stream of symbolic percepts (JSON) on sub_endpoint.
//   PUSH: sends action commands (JSON) on push_endpoint.
class BodyLink {
public:
    // Two-endpoint constructor: explicit SUB and PUSH endpoints.
    BodyLink(const std::string& sub_endpoint, const std::string& push_endpoint);

    // Single-endpoint constructor (backwards compat): derives PUSH port as
    // SUB port + 1.  e.g. tcp://127.0.0.1:5555 → push on :5556.
    explicit BodyLink(const std::string& sub_endpoint);

    ~BodyLink();

    BodyLink(const BodyLink&) = delete;
    BodyLink& operator=(const BodyLink&) = delete;

    void connect();
    void disconnect();

    // Non-blocking read of the latest percept. Returns nullopt if none ready.
    std::optional<Percept> poll_percept();

    // Send a JSON action string to the body.
    void send_action(const std::string& action_json);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace prometheus
