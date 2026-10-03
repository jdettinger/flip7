#pragma once

#include "flip7/env/deck.hpp"
#include "flip7/env/observation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace flip7 {

class Environment {
public:
    explicit Environment(std::uint64_t seed, int target_score = 200);

    Observation observe() const;
    Observation step(Command command);
    bool done() const noexcept;
    EpisodeResult result() const;
    const std::vector<RoundResult>& round_results() const noexcept;

private:
    void draw_cards(std::size_t count);
    void draw_one();
    void finish_round(RoundEnd reason);
    int calculate_round_score() const noexcept;

    Deck deck_;
    std::uint64_t seed_;
    std::uint64_t round_number_{1};
    int target_score_;
    int total_score_{0};
    std::array<Card, MAX_ROUND_CARDS> cards_{};
    std::size_t card_count_{0};
    std::array<std::uint8_t, MAX_NUMBER + 1> seen_numbers_{};
    int number_sum_{0};
    int modifier_sum_{0};
    std::size_t round_cards_drawn_{0};
    std::uint64_t actions_{0};
    std::uint64_t total_cards_drawn_{0};
    std::uint64_t busts_{0};
    std::uint64_t flip7s_{0};
    std::uint64_t voluntary_finishes_{0};
    bool has_second_chance_{false};
    bool doubled_{false};
    bool busted_{false};
    bool flip7_{false};
    bool round_finished_{false};
    bool game_finished_{false};
    GameEnd game_end_{GameEnd::DeckExhausted};
    std::vector<RoundResult> round_results_;
};

} // namespace flip7
