#include "flip7/simulation.hpp"

#include "flip7/env/environment.hpp"

namespace flip7 {

EpisodeResult run_episode(
    std::uint64_t seed,
    int target_score,
    Strategy& strategy
) {
    Environment environment(seed, target_score);
    while (!environment.done()) {
        const Observation observation = environment.observe();
        environment.step(strategy.act(observation));
    }
    return environment.result();
}

} // namespace flip7
