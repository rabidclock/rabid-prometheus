#include "arbiter/arbiter.h"

#include <iostream>

namespace prometheus {

Arbiter::Arbiter(Lizard& lizard, Soul& soul, BodyLink& body)
    : lizard_(lizard), soul_(soul), body_(body) {}

void Arbiter::submit_reflex(Reflex reflex) {
    std::lock_guard lock(reflex_mu_);
    // Keep the most urgent reflex.
    if (!pending_reflex_ || reflex.urgency > pending_reflex_->urgency) {
        pending_reflex_ = std::move(reflex);
    }
}

void Arbiter::submit_plan(SoulPlan plan) {
    std::lock_guard lock(plan_mu_);
    pending_plan_ = std::move(plan);
}

std::optional<SoulQuery> Arbiter::next_soul_query() {
    std::lock_guard lock(query_mu_);
    if (soul_queries_.empty()) return std::nullopt;
    auto q = std::move(soul_queries_.front());
    soul_queries_.pop();
    return q;
}

void Arbiter::escalate(const std::string& prompt,
                       std::optional<std::string> image_b64) {
    std::lock_guard lock(query_mu_);
    SoulQuery q;
    q.prompt   = prompt;
    q.image_b64 = std::move(image_b64);
    soul_queries_.push(std::move(q));
}

void Arbiter::dispatch_tick() {
    // ── Grab candidates ─────────────────────────────────────────
    std::optional<Reflex>   reflex;
    std::optional<SoulPlan> plan;

    {
        std::lock_guard lock(reflex_mu_);
        reflex.swap(pending_reflex_);
    }
    {
        std::lock_guard lock(plan_mu_);
        plan.swap(pending_plan_);
    }

    // ── Subsumption resolution ──────────────────────────────────
    // Layer 0 (Reflex/Avoid) vetoes Layer 2 (Soul) unless Soul
    // explicitly overrides.
    if (reflex && reflex->vetoes_soul) {
        if (plan && plan->override_safety) {
            std::cout << "[ARBITER] Soul OVERRIDE accepted.\n";
            body_.send_action(plan->action_json);
        } else {
            body_.send_action(reflex->action_json);
        }
        return;
    }

    // Prefer the Soul's plan when available, else fall back to reflex.
    if (plan) {
        body_.send_action(plan->action_json);
    } else if (reflex) {
        body_.send_action(reflex->action_json);
    }
    // else: nothing to do this tick.
}

} // namespace prometheus
