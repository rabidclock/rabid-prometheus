#pragma once

#include "lizard/lizard.h"
#include "soul/soul.h"
#include "ipc/body_link.h"

#include <mutex>
#include <optional>
#include <queue>

namespace prometheus {

// Subsumption Arbiter — resolves conflicts between Lizard and Soul.
//   Layer 0 (Reflex/Avoid) always wins unless the Soul explicitly
//   issues an [OVERRIDE: IGNORE_SAFETY] token.
class Arbiter {
public:
    Arbiter(Lizard& lizard, Soul& soul, BodyLink& body);

    // Called from the lizard thread.
    void submit_reflex(Reflex reflex);

    // Called from the soul thread.
    void submit_plan(SoulPlan plan);

    // Returns the next pending query for the Soul (thread-safe).
    std::optional<SoulQuery> next_soul_query();

    // Escalate a percept to the Soul for deeper reasoning.
    void escalate(const std::string& prompt,
                  std::optional<std::string> image_b64 = std::nullopt);

    // Main-thread tick: pick the highest-priority action and send to body.
    void dispatch_tick();

private:
    Lizard&   lizard_;
    Soul&     soul_;
    BodyLink& body_;

    std::mutex              reflex_mu_;
    std::optional<Reflex>   pending_reflex_;

    std::mutex              plan_mu_;
    std::optional<SoulPlan> pending_plan_;

    std::mutex              query_mu_;
    std::queue<SoulQuery>   soul_queries_;
};

} // namespace prometheus
