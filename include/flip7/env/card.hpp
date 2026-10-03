#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>

namespace flip7 {

enum class CardType : std::uint8_t {
    Number,
    SecondChance,
    Freeze,
    FlipThree,
    Modifier,
    Multiplier
};

struct Card {
    CardType type{CardType::Number};
    std::uint8_t value{0};

    constexpr bool operator==(const Card& other) const noexcept {
        return type == other.type && value == other.value;
    }
};

inline constexpr std::uint8_t MAX_NUMBER = 12;
inline constexpr std::size_t DECK_SIZE = 94;
inline constexpr std::size_t MAX_ROUND_CARDS = 20;
inline constexpr int FLIP7_BONUS = 15;
inline constexpr const char* RULESET_VERSION = "flip7-solo-v1";

std::string_view card_type_name(CardType type) noexcept;

} // namespace flip7
