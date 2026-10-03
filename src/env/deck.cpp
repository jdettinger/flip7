#include "flip7/env/deck.hpp"

#include <stdexcept>

namespace flip7 {
namespace {

class SplitMix64 {
public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}

    std::uint64_t next() noexcept {
        std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    }

    std::size_t bounded(std::size_t bound) noexcept {
        const auto width = static_cast<std::uint64_t>(bound);
        const std::uint64_t threshold = (0U - width) % width;
        std::uint64_t value = next();
        while (value < threshold) {
            value = next();
        }
        return static_cast<std::size_t>(value % width);
    }

private:
    std::uint64_t state_;
};

} // namespace

Deck::Deck(std::uint64_t seed) {
    std::size_t position = 0;
    auto add = [&](Card card) {
        cards_[position++] = card;
        if (card.type == CardType::Number) {
            ++remaining_numbers_[card.value];
        }
    };

    add({CardType::Number, 0});
    for (std::uint8_t value = 1; value <= MAX_NUMBER; ++value) {
        for (std::uint8_t copy = 0; copy < value; ++copy) {
            add({CardType::Number, value});
        }
    }
    for (std::size_t copy = 0; copy < 3; ++copy) {
        add({CardType::SecondChance, 0});
        add({CardType::Freeze, 0});
        add({CardType::FlipThree, 0});
    }
    for (std::uint8_t value = 2; value <= 10; value += 2) {
        add({CardType::Modifier, value});
    }
    add({CardType::Multiplier, 2});

    if (position != cards_.size()) {
        throw std::logic_error("Flip 7 card composition has an invalid size");
    }

    SplitMix64 random(seed);
    for (std::size_t remaining = cards_.size(); remaining > 1; --remaining) {
        const std::size_t other = random.bounded(remaining);
        const Card temporary = cards_[remaining - 1];
        cards_[remaining - 1] = cards_[other];
        cards_[other] = temporary;
    }
}

bool Deck::empty() const noexcept {
    return position_ == cards_.size();
}

std::size_t Deck::remaining_count() const noexcept {
    return cards_.size() - position_;
}

const std::array<std::uint8_t, MAX_NUMBER + 1>&
Deck::remaining_number_counts() const noexcept {
    return remaining_numbers_;
}

Card Deck::draw() {
    if (empty()) {
        throw std::out_of_range("Cannot draw from an empty Flip 7 deck");
    }
    const Card card = cards_[position_++];
    if (card.type == CardType::Number) {
        --remaining_numbers_[card.value];
    }
    return card;
}

} // namespace flip7
