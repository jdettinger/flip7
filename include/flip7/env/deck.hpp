#pragma once

#include "flip7/env/card.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace flip7 {

class Deck {
public:
    explicit Deck(std::uint64_t seed);

    bool empty() const noexcept;
    std::size_t remaining_count() const noexcept;
    const std::array<std::uint8_t, MAX_NUMBER + 1>&
    remaining_number_counts() const noexcept;
    Card draw();

private:
    std::array<Card, DECK_SIZE> cards_{};
    std::array<std::uint8_t, MAX_NUMBER + 1> remaining_numbers_{};
    std::size_t position_{0};
};

} // namespace flip7
