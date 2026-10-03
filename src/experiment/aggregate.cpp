#include "flip7/experiment/csv.hpp"

#include <map>
#include <stdexcept>

namespace flip7::experiment {
namespace {

void validate_same_job(const SimulationResult& left, const SimulationResult& right) {
    if (left.job_id != right.job_id ||
        left.strategy_name != right.strategy_name ||
        left.strategy_parameters != right.strategy_parameters ||
        left.target_score != right.target_score) {
        throw std::invalid_argument("Cannot aggregate results from different strategies");
    }
    if (left.seed_start > UINT64_MAX - left.episode_count ||
        left.seed_start + left.episode_count != right.seed_start) {
        throw std::invalid_argument("Simulation result batches are not contiguous");
    }
}

void merge(SimulationResult& target, const SimulationResult& source) {
    validate_same_job(target, source);
    target.episode_count += source.episode_count;
    target.rounds_played += source.rounds_played;
    target.actions += source.actions;
    target.cards_drawn += source.cards_drawn;
    target.busts += source.busts;
    target.flip7s += source.flip7s;
    target.voluntary_finishes += source.voluntary_finishes;
    target.target_reached_episodes += source.target_reached_episodes;
    target.deck_exhausted_episodes += source.deck_exhausted_episodes;
    target.final_score_sum += source.final_score_sum;
    target.final_score_square_sum += source.final_score_square_sum;
    for (const auto& [score, count] : source.final_score_distribution) {
        target.final_score_distribution[score] += count;
    }
    for (const auto& [score, count] : source.round_score_distribution) {
        target.round_score_distribution[score] += count;
    }
    for (std::size_t index = 0; index < target.round_end_distribution.size(); ++index) {
        target.round_end_distribution[index] += source.round_end_distribution[index];
    }
}

} // namespace

std::vector<SimulationResult> aggregate_results(
    const std::vector<SimulationResult>& batches
) {
    std::map<std::size_t, SimulationResult> grouped;
    for (const SimulationResult& batch : batches) {
        const auto found = grouped.find(batch.job_id);
        if (found == grouped.end()) {
            grouped.emplace(batch.job_id, batch);
        } else {
            merge(found->second, batch);
        }
    }

    std::vector<SimulationResult> results;
    results.reserve(grouped.size());
    for (auto& [job_id, result] : grouped) {
        (void)job_id;
        results.push_back(std::move(result));
    }
    return results;
}

} // namespace flip7::experiment
