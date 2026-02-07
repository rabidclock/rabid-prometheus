#include "lizard/lizard.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

// TODO: #include "llama.h" — uncomment once llama.cpp is linked
// #include "llama.h"

namespace prometheus {

// ── Pimpl ───────────────────────────────────────────────────────
struct Lizard::Impl {
    // llama_model*   model   = nullptr;
    // llama_context* ctx     = nullptr;
    std::string model_path;
    bool        loaded{false};
};

Lizard::Lizard(Memory& mem)
    : impl_(std::make_unique<Impl>())
    , memory_(mem) {}

Lizard::~Lizard() {
    // if (impl_->ctx)   llama_free(impl_->ctx);
    // if (impl_->model) llama_free_model(impl_->model);
}

void Lizard::load_model(const std::string& model_path) {
    impl_->model_path = model_path;
    std::cout << "[LIZARD] Loading Phi-3.5 from " << model_path << "...\n";

    // ── llama.cpp init (stubbed) ────────────────────────────────
    // llama_backend_init();
    // auto params = llama_model_default_params();
    // params.n_gpu_layers = 99;   // offload everything
    // impl_->model = llama_load_model_from_file(model_path.c_str(), params);
    // if (!impl_->model) throw std::runtime_error("Failed to load Phi-3.5");
    //
    // auto cparams   = llama_context_default_params();
    // cparams.n_ctx  = 2048;     // small context for speed
    // impl_->ctx = llama_new_context_with_model(impl_->model, cparams);

    impl_->loaded = true;
    std::cout << "[LIZARD] Model loaded (stub).\n";
}

Reflex Lizard::react(const Percept& percept) {
    auto t0 = std::chrono::steady_clock::now();

    Reflex reflex{};
    reflex.layer = Reflex::Layer::Tactic;

    // ── Layer 0: hard-coded survival reflexes (no LLM needed) ──
    if (percept.health < 4.0f) {
        reflex.layer       = Reflex::Layer::Avoid;
        reflex.action_json = R"({"action":"flee","reason":"critical_health"})";
        reflex.urgency     = 1.0f;
        reflex.vetoes_soul = true;
        memory_.tag("[MEM:NEAR_DEATH]");
        return reflex;
    }

    if (percept.hostile_nearby) {
        reflex.layer       = Reflex::Layer::Avoid;
        reflex.action_json = R"({"action":"flee","reason":"hostile_nearby"})";
        reflex.urgency     = 0.9f;
        reflex.vetoes_soul = true;
        return reflex;
    }

    if (percept.hunger < 6.0f) {
        reflex.action_json = R"({"action":"eat","reason":"hungry"})";
        reflex.urgency     = 0.5f;
        return reflex;
    }

    // ── Layer 1: LLM-assisted tactical decision (stubbed) ──────
    // Build prompt from percept.raw_json + recent memory markers,
    // tokenise, run inference, parse JSON action.
    //
    // std::string prompt = build_tactical_prompt(percept, memory_);
    // std::string output = llama_generate(impl_->ctx, prompt, /*max_tokens=*/64);
    // reflex = parse_reflex(output);

    reflex.action_json = R"({"action":"idle","reason":"no_threat"})";
    reflex.urgency     = 0.0f;

    auto dt = std::chrono::steady_clock::now() - t0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
    if (ms > 100) {
        std::cerr << "[LIZARD] WARNING: react() took " << ms << " ms (target <100)\n";
    }

    return reflex;
}

} // namespace prometheus
