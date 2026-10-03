#include "flip7/env/environment.hpp"

#include <algorithm>
#include <stdexcept>

namespace flip7 {

Environment::Environment(std::uint64_t seed, int target_score)
    : deck_(seed), seed_(seed), target_score_(target_score) {
    if (target_score_ <= 0) {
        throw std::invalid_argument("Target score must be positive");
    }
    round_results_.reserve(DECK_SIZE);
}

Observation Environment::observe() const {
    Observation observation;
    observation.round_number = round_number_;
    observation.target_score = target_score_;
    observation.total_score = total_score_;
    observation.round_score = busted_ ? 0 : calculate_round_score();
    observation.number_sum = number_sum_;
    observation.modifier_sum = modifier_sum_;
    observation.card_count = card_count_;
    observation.unique_number_count = static_cast<std::size_t>(
        std::count_if(
            seen_numbers_.begin(),
            seen_numbers_.end(),
            [](std::uint8_t count) { return count != 0; }
        )
    );
    observation.remaining_card_count = deck_.remaining_count();
    observation.cards = cards_;
    observation.seen_numbers = seen_numbers_;
    observation.remaining_numbers = deck_.remaining_number_counts();
    observation.has_second_chance = has_second_chance_;
    observation.doubled = doubled_;
    observation.busted = busted_;
    observation.flip7 = flip7_;
    observation.round_finished = round_finished_;
    observation.game_finished = game_finished_;
    return observation;
}

Observation Environment::step(Command command) {
    if (game_finished_) {
        throw std::logic_error("Cannot act after the episode has finished");
    }
    if (round_finished_) {
        throw std::logic_error("Cannot act after the round has finished");
    }
    if (command == Command::Finish && round_cards_drawn_ == 0) {
        throw std::invalid_argument("Cannot finish a round before drawing a card");
    }

    switch (command) {
    case Command::Finish:
        if (round_cards_drawn_ == 0) {
            throw std::invalid_argument("Cannot finish a round before drawing a card");
        }
        ++actions_;
        finish_round(RoundEnd::Finish);
        break;
    case Command::Hit:
        ++actions_;
        draw_cards(1);
        break;
    default:
        throw std::invalid_argument("Unknown Flip 7 command");
    }
    return observe();
}

bool Environment::done() const noexcept {
    return game_finished_;
}

EpisodeResult Environment::result() const {
    if (!game_finished_) {
        throw std::logic_error("Episode result requested before game completion");
    }

    EpisodeResult output;
    output.seed = seed_;
    output.final_score = total_score_;
    output.target_score = target_score_;
    output.rounds = round_results_.size();
    output.actions = actions_;
    output.cards_drawn = total_cards_drawn_;
    output.busts = busts_;
    output.flip7s = flip7s_;
    output.voluntary_finishes = voluntary_finishes_;
    output.round_results = round_results_;
    output.target_reached = game_end_ == GameEnd::TargetScore;
    output.end = game_end_;
    return output;
}

const std::vector<RoundResult>& Environment::round_results() const noexcept {
    return round_results_;
}

void Environment::draw_cards(std::size_t count) {
    while (count > 0 && !round_finished_) {
        --count;
        draw_one();
    }
}

void Environment::draw_one() {
    if (deck_.empty()) {
        finish_round(RoundEnd::DeckExhausted);
        return;
    }

    const Card card = deck_.draw();
    ++round_cards_drawn_;
    ++total_cards_drawn_;

    if (card.type == CardType::Number && seen_numbers_[card.value] != 0) {
        if (has_second_chance_) {
            has_second_chance_ = false;
            const auto end = cards_.begin() + static_cast<std::ptrdiff_t>(card_count_);
            const auto second_chance = std::find_if(
                cards_.begin(),
                end,
                [](const Card& held) {
                    return held.type == CardType::SecondChance;
                }
            );
            if (second_chance != end) {
                std::move(second_chance + 1, end, second_chance);
                --card_count_;
            }
            return;
        }
        busted_ = true;
        finish_round(RoundEnd::Bust);
        return;
    }

    if (card.type == CardType::SecondChance && has_second_chance_) {
        return;
    }
    if (card_count_ >= cards_.size()) {
        throw std::logic_error("Round card capacity exceeded");
    }
    cards_[card_count_++] = card;

    switch (card.type) {
    case CardType::Number:
        seen_numbers_[card.value] = 1;
        number_sum_ += card.value;
        if (std::count(seen_numbers_.begin(), seen_numbers_.end(), 1) == 7) {
            flip7_ = true;
            finish_round(RoundEnd::Flip7);
        }
        break;
    case CardType::SecondChance:
        has_second_chance_ = true;
        break;
    case CardType::Freeze:
        finish_round(RoundEnd::Freeze);
        break;
    case CardType::FlipThree:
        draw_cards(3);
        break;
    case CardType::Modifier:
        modifier_sum_ += card.value;
        break;
    case CardType::Multiplier:
        doubled_ = true;
        break;
    }
}

void Environment::finish_round(RoundEnd reason) {
    if (round_finished_) {
        return;
    }

    round_finished_ = true;
    const int score = busted_ ? 0 : calculate_round_score();
    total_score_ += score;

    RoundResult round;
    round.round_number = round_number_;
    round.score = score;
    round.cards_drawn = round_cards_drawn_;
    round.end = reason;
    round.busted = busted_;
    round.flip7 = flip7_;
    round_results_.push_back(round);

    if (busted_) {
        ++busts_;
    }
    if (flip7_) {
        ++flip7s_;
    }
    if (reason == RoundEnd::Finish) {
        ++voluntary_finishes_;
    }

    if (total_score_ >= target_score_) {
        game_finished_ = true;
        game_end_ = GameEnd::TargetScore;
        return;
    }
    if (reason == RoundEnd::DeckExhausted || deck_.empty()) {
        game_finished_ = true;
        game_end_ = GameEnd::DeckExhausted;
        return;
    }

    ++round_number_;
    cards_.fill({});
    card_count_ = 0;
    seen_numbers_.fill(0);
    number_sum_ = 0;
    modifier_sum_ = 0;
    round_cards_drawn_ = 0;
    has_second_chance_ = false;
    doubled_ = false;
    busted_ = false;
    flip7_ = false;
    round_finished_ = false;
}

int Environment::calculate_round_score() const noexcept {
    const int subtotal = number_sum_ + modifier_sum_;
    return (doubled_ ? subtotal * 2 : subtotal) + (flip7_ ? FLIP7_BONUS : 0);
}

} // namespace flip7
