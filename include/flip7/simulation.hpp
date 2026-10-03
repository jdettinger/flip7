#pragma once

#include "flip7/env/observation.hpp"
#include "flip7/player/strategy.hpp"

#include <cstdint>

namespace flip7 {

EpisodeResult run_episode(
    std::uint64_t seed,
    int target_score,
    Strategy& strategy
);

} // namespace flip7
