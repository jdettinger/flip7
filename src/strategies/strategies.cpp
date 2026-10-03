#include "flip7/strategies/strategies.hpp"

#include <cmath>
#include <stdexcept>

namespace flip7 {
namespace {

Command threshold_decision(bool should_finish, const Observation& observation) {
    return should_finish || observation.remaining_card_count == 0
        ? Command::Finish
        : Command::Hit;
}

} // namespace

CardCountStrategy::CardCountStrategy(std::size_t target_cards)
    : target_cards_(target_cards) {
    if (target_cards_ == 0) {
        throw std::invalid_argument("Card-count target must be positive");
    }
}

Command CardCountStrategy::act(const Observation& observation) {
    return threshold_decision(observation.card_count >= target_cards_, observation);
}

std::string_view CardCountStrategy::name() const noexcept {
    return "card_count";
}

CardSumStrategy::CardSumStrategy(int target_sum) : target_sum_(target_sum) {
    if (target_sum_ <= 0) {
        throw std::invalid_argument("Card-sum target must be positive");
    }
}

Command CardSumStrategy::act(const Observation& observation) {
    return threshold_decision(observation.round_score >= target_sum_, observation);
}

std::string_view CardSumStrategy::name() const noexcept {
    return "card_sum";
}

BustProbabilityStrategy::BustProbabilityStrategy(double max_probability)
    : max_probability_(max_probability) {
    if (!std::isfinite(max_probability_) ||
        max_probability_ < 0.0 ||
        max_probability_ > 1.0) {
        throw std::invalid_argument("Bust probability threshold must be in [0, 1]");
    }
}

Command BustProbabilityStrategy::act(const Observation& observation) {
    return threshold_decision(
        estimated_bust_probability(observation) > max_probability_,
        observation
    );
}

std::string_view BustProbabilityStrategy::name() const noexcept {
    return "bust_probability";
}

UniqueNumberStrategy::UniqueNumberStrategy(std::size_t target_unique)
    : target_unique_(target_unique) {
    if (target_unique_ == 0 || target_unique_ > 7) {
        throw std::invalid_argument("Unique-number target must be between 1 and 7");
    }
}

Command UniqueNumberStrategy::act(const Observation& observation) {
    return threshold_decision(
        observation.unique_number_count >= target_unique_,
        observation
    );
}

std::string_view UniqueNumberStrategy::name() const noexcept {
    return "unique_number";
}

HybridStrategy::HybridStrategy(
    int target_sum,
    std::size_t target_unique
)
    : target_sum_(target_sum),
      target_unique_(target_unique) {
    if (target_sum_ <= 0) {
        throw std::invalid_argument("Hybrid score target must be positive");
    }
    if (target_unique_ == 0 || target_unique_ > 7) {
        throw std::invalid_argument("Hybrid unique target must be between 1 and 7");
    }
}

Command HybridStrategy::act(const Observation& observation) {
    const bool stop =
        observation.round_score >= target_sum_ ||
        observation.unique_number_count >= target_unique_;
    return threshold_decision(stop, observation);
}

std::string_view HybridStrategy::name() const noexcept {
    return "hybrid";
}

double estimated_bust_probability(const Observation& observation) noexcept {
    if (observation.remaining_card_count == 0 || observation.has_second_chance) {
        return 0.0;
    }
    std::size_t duplicate_cards = 0;
    for (std::size_t value = 0; value < observation.seen_numbers.size(); ++value) {
        if (observation.seen_numbers[value] != 0) {
            duplicate_cards += observation.remaining_numbers[value];
        }
    }
    return static_cast<double>(duplicate_cards) /
        static_cast<double>(observation.remaining_card_count);
}

} // namespace flip7
