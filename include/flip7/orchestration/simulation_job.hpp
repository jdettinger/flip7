#pragma once

#include "flip7/player/strategy.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace flip7::orchestration {

using StrategyFactory = std::function<std::unique_ptr<Strategy>()>;

struct SimulationJob {
    std::size_t job_id{0};
    std::string strategy_name;
    std::string strategy_parameters;
    std::uint64_t seed_start{0};
    std::uint64_t episode_count{0};
    int target_score{200};
    StrategyFactory strategy_factory;
};

struct SimulationResult {
    std::size_t job_id{0};
    std::string strategy_name;
    std::string strategy_parameters;
    std::uint64_t seed_start{0};
    std::uint64_t episode_count{0};
    int target_score{200};

    std::uint64_t rounds_played{0};
    std::uint64_t actions{0};
    std::uint64_t cards_drawn{0};
    std::uint64_t busts{0};
    std::uint64_t flip7s{0};
    std::uint64_t voluntary_finishes{0};
    std::uint64_t target_reached_episodes{0};
    std::uint64_t deck_exhausted_episodes{0};
    long double final_score_sum{0.0L};
    long double final_score_square_sum{0.0L};
    std::map<int, std::uint64_t> final_score_distribution;
    std::map<int, std::uint64_t> round_score_distribution;
    std::array<std::uint64_t, 5> round_end_distribution{};
};

} // namespace flip7::orchestration
