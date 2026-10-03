#include "flip7/env/card.hpp"

namespace flip7 {

std::string_view card_type_name(CardType type) noexcept {
    switch (type) {
    case CardType::Number:
        return "number";
    case CardType::SecondChance:
        return "second_chance";
    case CardType::Freeze:
        return "freeze";
    case CardType::FlipThree:
        return "flip_three";
    case CardType::Modifier:
        return "modifier";
    case CardType::Multiplier:
        return "multiplier";
    }
    return "unknown";
}

} // namespace flip7
