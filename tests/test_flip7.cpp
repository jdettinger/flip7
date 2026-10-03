#include "flip7/env/deck.hpp"
#include "flip7/env/environment.hpp"
#include "flip7/experiment/csv.hpp"
#include "flip7/orchestration/parallel_runner.hpp"
#include "flip7/simulation.hpp"
#include "flip7/strategies/strategies.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

class AlwaysHit final : public flip7::Strategy {
public:
    flip7::Command act(const flip7::Observation&) override {
        return flip7::Command::Hit;
    }

    std::string_view name() const noexcept override {
        return "always_hit";
    }
};

std::uint64_t find_seed(
    const std::function<bool(flip7::Deck&)>& matches
) {
    for (std::uint64_t seed = 0; seed < 100000; ++seed) {
        flip7::Deck deck(seed);
        if (matches(deck)) {
            return seed;
        }
    }
    throw std::runtime_error("Could not find a deterministic test deck");
}

void test_deck_composition_and_reproducibility() {
    flip7::Deck first(123456);
    flip7::Deck second(123456);
    std::array<std::uint8_t, 13> counts{};
    std::array<std::size_t, 6> types{};
    std::array<std::uint8_t, 11> modifiers{};
    std::size_t cards = 0;
    while (!first.empty()) {
        const auto left = first.draw();
        const auto right = second.draw();
        assert(left == right);
        if (left.type == flip7::CardType::Number) {
            ++counts[left.value];
        }
        ++types[static_cast<std::size_t>(left.type)];
        if (left.type == flip7::CardType::Modifier) {
            ++modifiers[left.value];
        }
        ++cards;
    }
    assert(cards == flip7::DECK_SIZE);
    assert(counts[0] == 1);
    for (std::size_t value = 1; value <= flip7::MAX_NUMBER; ++value) {
        assert(counts[value] == value);
    }
    assert(types[static_cast<std::size_t>(flip7::CardType::SecondChance)] == 3);
    assert(types[static_cast<std::size_t>(flip7::CardType::Freeze)] == 3);
    assert(types[static_cast<std::size_t>(flip7::CardType::FlipThree)] == 3);
    assert(types[static_cast<std::size_t>(flip7::CardType::Modifier)] == 5);
    assert(types[static_cast<std::size_t>(flip7::CardType::Multiplier)] == 1);
    for (const std::size_t value : {2U, 4U, 6U, 8U, 10U}) {
        assert(modifiers[value] == 1);
    }
    assert(second.empty());
}

void test_empty_finish_is_rejected_and_deck_exhaustion_terminates() {
    flip7::Environment environment(42, 100000);
    bool rejected = false;
    try {
        environment.step(flip7::Command::Finish);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    AlwaysHit strategy;
    const auto result = flip7::run_episode(42, 100000, strategy);
    assert(!result.target_reached);
    assert(result.end == flip7::GameEnd::DeckExhausted);
    assert(result.cards_drawn <= flip7::DECK_SIZE);
    assert(result.rounds > 0);
}

void test_reproducible_episodes() {
    AlwaysHit first_strategy;
    AlwaysHit second_strategy;
    const auto first = flip7::run_episode(987654, 200, first_strategy);
    const auto second = flip7::run_episode(987654, 200, second_strategy);
    assert(first.final_score == second.final_score);
    assert(first.rounds == second.rounds);
    assert(first.actions == second.actions);
    assert(first.cards_drawn == second.cards_drawn);
    assert(first.busts == second.busts);
    assert(first.flip7s == second.flip7s);
    assert(first.end == second.end);
}

void test_environment_rules_and_scoring() {
    const auto scored_seed = find_seed([](flip7::Deck& deck) {
        const auto modifier = deck.draw();
        const auto number = deck.draw();
        const auto multiplier = deck.draw();
        return modifier.type == flip7::CardType::Modifier &&
            number.type == flip7::CardType::Number &&
            multiplier.type == flip7::CardType::Multiplier;
    });
    flip7::Deck scored_deck(scored_seed);
    const auto modifier = scored_deck.draw();
    const auto number = scored_deck.draw();
    flip7::Environment scored(scored_seed, 1000);
    scored.step(flip7::Command::Hit);
    scored.step(flip7::Command::Hit);
    const auto doubled = scored.step(flip7::Command::Hit);
    assert(doubled.round_score == (modifier.value + number.value) * 2);
    scored.step(flip7::Command::Finish);
    assert(scored.round_results().front().score ==
        (modifier.value + number.value) * 2);

    const auto flip7_seed = find_seed([](flip7::Deck& deck) {
        std::array<bool, flip7::MAX_NUMBER + 1> seen{};
        for (std::size_t index = 0; index < 7; ++index) {
            const auto card = deck.draw();
            if (card.type != flip7::CardType::Number || seen[card.value]) {
                return false;
            }
            seen[card.value] = true;
        }
        return true;
    });
    flip7::Deck flip7_deck(flip7_seed);
    int number_sum = 0;
    for (std::size_t index = 0; index < 7; ++index) {
        number_sum += flip7_deck.draw().value;
    }
    flip7::Environment flip7_environment(flip7_seed, 1000);
    flip7::Observation after_flip7;
    for (std::size_t index = 0; index < 7; ++index) {
        after_flip7 = flip7_environment.step(flip7::Command::Hit);
    }
    assert(flip7_environment.round_results().front().end == flip7::RoundEnd::Flip7);
    assert(flip7_environment.round_results().front().score ==
        number_sum + flip7::FLIP7_BONUS);
    assert(after_flip7.round_number == 2);

    const auto second_chance_seed = find_seed([](flip7::Deck& deck) {
        const auto number = deck.draw();
        const auto second_chance = deck.draw();
        const auto duplicate = deck.draw();
        return number.type == flip7::CardType::Number &&
            second_chance.type == flip7::CardType::SecondChance &&
            duplicate.type == flip7::CardType::Number &&
            duplicate.value == number.value;
    });
    flip7::Deck second_chance_deck(second_chance_seed);
    const auto kept_number = second_chance_deck.draw();
    flip7::Environment second_chance(second_chance_seed, 1000);
    second_chance.step(flip7::Command::Hit);
    second_chance.step(flip7::Command::Hit);
    const auto saved = second_chance.step(flip7::Command::Hit);
    assert(!saved.busted);
    assert(!saved.has_second_chance);
    assert(saved.unique_number_count == 1);
    assert(saved.round_score == kept_number.value);

    const auto bust_seed = find_seed([](flip7::Deck& deck) {
        const auto first = deck.draw();
        const auto second = deck.draw();
        return first.type == flip7::CardType::Number &&
            second.type == flip7::CardType::Number &&
            first.value == second.value;
    });
    flip7::Environment busting(bust_seed, 1000);
    busting.step(flip7::Command::Hit);
    busting.step(flip7::Command::Hit);
    assert(busting.round_results().front().busted);
    assert(busting.round_results().front().score == 0);

    const auto flip_three_seed = find_seed([](flip7::Deck& deck) {
        if (deck.draw().type != flip7::CardType::FlipThree) {
            return false;
        }
        for (std::size_t index = 0; index < 3; ++index) {
            if (deck.draw().type != flip7::CardType::Number) {
                return false;
            }
        }
        return true;
    });
    flip7::Environment flip_three(flip_three_seed, 1000);
    const auto after_forced_draws = flip_three.step(flip7::Command::Hit);
    assert(after_forced_draws.card_count == 4);
    assert(after_forced_draws.unique_number_count == 3);

    const auto freeze_seed = find_seed([](flip7::Deck& deck) {
        return deck.draw().type == flip7::CardType::Freeze;
    });
    flip7::Environment frozen(freeze_seed, 1000);
    frozen.step(flip7::Command::Hit);
    assert(frozen.round_results().front().end == flip7::RoundEnd::Freeze);
}

void test_strategy_thresholds_and_bust_estimate() {
    flip7::Observation observation;
    observation.card_count = 3;
    observation.unique_number_count = 2;
    observation.round_score = 16;
    observation.remaining_card_count = 10;
    observation.seen_numbers[2] = 1;
    observation.remaining_numbers[2] = 2;

    flip7::CardCountStrategy count(3);
    flip7::CardSumStrategy sum(15);
    flip7::BustProbabilityStrategy probability(0.19);
    flip7::UniqueNumberStrategy unique(3);
    flip7::HybridStrategy hybrid_by_sum(16, 7);
    flip7::HybridStrategy hybrid_by_unique(100, 2);
    assert(count.act(observation) == flip7::Command::Finish);
    assert(sum.act(observation) == flip7::Command::Finish);
    assert(probability.act(observation) == flip7::Command::Finish);
    assert(unique.act(observation) == flip7::Command::Hit);
    assert(hybrid_by_sum.act(observation) == flip7::Command::Finish);
    assert(hybrid_by_unique.act(observation) == flip7::Command::Finish);
    assert(flip7::estimated_bust_probability(observation) == 0.2);
    observation.has_second_chance = true;
    assert(flip7::estimated_bust_probability(observation) == 0.0);
}

void test_parallel_runner_and_aggregation() {
    std::vector<flip7::orchestration::SimulationJob> jobs;
    for (std::size_t batch = 0; batch < 2; ++batch) {
        flip7::orchestration::SimulationJob job;
        job.job_id = 0;
        job.strategy_name = "card_count";
        job.strategy_parameters = "target_cards=4";
        job.seed_start = 1000 + batch * 5;
        job.episode_count = 5;
        job.target_score = 100;
        job.strategy_factory = [] {
            return std::make_unique<flip7::CardCountStrategy>(4);
        };
        jobs.push_back(std::move(job));
    }

    const auto batches = flip7::orchestration::ParallelRunner(2).run(jobs);
    const auto results = flip7::experiment::aggregate_results(batches);
    assert(results.size() == 1);
    assert(results[0].episode_count == 10);
    assert(results[0].seed_start == 1000);
    assert(results[0].target_reached_episodes +
        results[0].deck_exhausted_episodes == results[0].episode_count);
}

} // namespace

int main() {
    test_deck_composition_and_reproducibility();
    test_empty_finish_is_rejected_and_deck_exhaustion_terminates();
    test_reproducible_episodes();
    test_environment_rules_and_scoring();
    test_strategy_thresholds_and_bust_estimate();
    test_parallel_runner_and_aggregation();
    std::cout << "All Flip 7 tests passed\n";
}
