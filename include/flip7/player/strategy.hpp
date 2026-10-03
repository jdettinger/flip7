#pragma once

#include "flip7/env/observation.hpp"

#include <string_view>

namespace flip7 {

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual Command act(const Observation& observation) = 0;
    virtual std::string_view name() const noexcept = 0;
};

} // namespace flip7
