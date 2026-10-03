#pragma once

#include "flip7/player/strategy.hpp"

#include <cstddef>

namespace flip7 {

class CardCountStrategy final : public Strategy {
public:
    explicit CardCountStrategy(std::size_t target_cards);
    Command act(const Observation& observation) override;
    std::string_view name() const noexcept override;

private:
    std::size_t target_cards_;
};

class CardSumStrategy final : public Strategy {
public:
    explicit CardSumStrategy(int target_sum);
    Command act(const Observation& observation) override;
    std::string_view name() const noexcept override;

private:
    int target_sum_;
};

class BustProbabilityStrategy final : public Strategy {
public:
    explicit BustProbabilityStrategy(double max_probability);
    Command act(const Observation& observation) override;
    std::string_view name() const noexcept override;

private:
    double max_probability_;
};

class UniqueNumberStrategy final : public Strategy {
public:
    explicit UniqueNumberStrategy(std::size_t target_unique);
    Command act(const Observation& observation) override;
    std::string_view name() const noexcept override;

private:
    std::size_t target_unique_;
};

class HybridStrategy final : public Strategy {
public:
    HybridStrategy(
        int target_sum,
        std::size_t target_unique
    );
    Command act(const Observation& observation) override;
    std::string_view name() const noexcept override;

private:
    int target_sum_;
    std::size_t target_unique_;
};

double estimated_bust_probability(
    const Observation& observation
) noexcept;

} // namespace flip7
