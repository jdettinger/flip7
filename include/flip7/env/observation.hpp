#pragma once

#include "flip7/env/card.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace flip7 {

struct Observation {
    std::uint64_t round_number{0};
    int target_score{0};
    int total_score{0};
    int round_score{0};
    int number_sum{0};
    int modifier_sum{0};
    std::size_t card_count{0};
    std::size_t unique_number_count{0};
    std::size_t remaining_card_count{0};
    std::array<Card, MAX_ROUND_CARDS> cards{};
    std::array<std::uint8_t, MAX_NUMBER + 1> seen_numbers{};
    std::array<std::uint8_t, MAX_NUMBER + 1> remaining_numbers{};
    bool has_second_chance{false};
    bool doubled{false};
    bool busted{false};
    bool flip7{false};
    bool round_finished{false};
    bool game_finished{false};
};

enum class Command : std::uint8_t {
    Hit,
    Finish
};

enum class RoundEnd : std::uint8_t {
    Bust,
    Flip7,
    Freeze,
    Finish,
    DeckExhausted
};

enum class GameEnd : std::uint8_t {
    TargetScore,
    DeckExhausted
};

struct RoundResult {
    std::uint64_t round_number{0};
    int score{0};
    std::size_t cards_drawn{0};
    RoundEnd end{RoundEnd::Finish};
    bool busted{false};
    bool flip7{false};
};

struct EpisodeResult {
    std::uint64_t seed{0};
    int final_score{0};
    int target_score{0};
    std::uint64_t rounds{0};
    std::uint64_t actions{0};
    std::uint64_t cards_drawn{0};
    std::uint64_t busts{0};
    std::uint64_t flip7s{0};
    std::uint64_t voluntary_finishes{0};
    std::vector<RoundResult> round_results;
    bool target_reached{false};
    GameEnd end{GameEnd::DeckExhausted};
};

} // namespace flip7
