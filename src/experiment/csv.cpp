#include "flip7/experiment/csv.hpp"

#include "flip7/env/observation.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>

namespace flip7::experiment {
namespace {

std::string quote_csv(const std::string& value) {
    std::string quoted = "\"";
    for (const char character : value) {
        if (character == '"') {
            quoted += "\"\"";
        } else {
            quoted += character;
        }
    }
    quoted += '"';
    return quoted;
}

double rate(std::uint64_t count, std::uint64_t denominator) {
    return denominator == 0
        ? 0.0
        : static_cast<double>(count) / static_cast<double>(denominator);
}

std::string end_point(const SimulationResult& result) {
    if (result.target_reached_episodes == result.episode_count) {
        return "target_score";
    }
    if (result.deck_exhausted_episodes == result.episode_count) {
        return "deck_exhausted";
    }
    return "mixed";
}

std::string_view round_end_name(std::size_t index) {
    switch (static_cast<RoundEnd>(index)) {
    case RoundEnd::Bust:
        return "bust";
    case RoundEnd::Flip7:
        return "flip7";
    case RoundEnd::Freeze:
        return "freeze";
    case RoundEnd::Finish:
        return "finish";
    case RoundEnd::DeckExhausted:
        return "deck_exhausted";
    }
    return "unknown";
}

std::filesystem::path distribution_path(const std::filesystem::path& results_path) {
    const std::string filename =
        results_path.stem().string() + "_round_distribution.csv";
    return results_path.parent_path() / filename;
}

} // namespace

void write_csv(
    const std::filesystem::path& results_path,
    const std::vector<SimulationResult>& results
) {
    std::ofstream output(results_path);
    if (!output) {
        throw std::runtime_error("Could not open results CSV: " + results_path.string());
    }
    output << "ruleset_version,strategy,parameters,target_score,episodes,seed_start,seed_end,"
              "mean_score,score_variance,mean_rounds,mean_actions,mean_cards_drawn,"
              "bust_rate,flip7_rate,finish_rate,target_reached_rate,end_point\n";
    output << std::setprecision(12);

    for (const SimulationResult& result : results) {
        if (result.episode_count == 0) {
            throw std::invalid_argument("Cannot write an empty simulation result");
        }
        const long double episodes =
            static_cast<long double>(result.episode_count);
        const long double mean_score = result.final_score_sum / episodes;
        const long double variance = std::max(
            0.0L,
            result.final_score_square_sum / episodes - mean_score * mean_score
        );
        const std::uint64_t seed_end =
            result.seed_start + result.episode_count - 1;
        output
            << RULESET_VERSION << ','
            << quote_csv(result.strategy_name) << ','
            << quote_csv(result.strategy_parameters) << ','
            << result.target_score << ','
            << result.episode_count << ','
            << result.seed_start << ','
            << seed_end << ','
            << static_cast<double>(mean_score) << ','
            << static_cast<double>(variance) << ','
            << rate(result.rounds_played, result.episode_count) << ','
            << rate(result.actions, result.episode_count) << ','
            << rate(result.cards_drawn, result.episode_count) << ','
            << rate(result.busts, result.rounds_played) << ','
            << rate(result.flip7s, result.rounds_played) << ','
            << rate(result.voluntary_finishes, result.rounds_played) << ','
            << rate(result.target_reached_episodes, result.episode_count) << ','
            << end_point(result) << '\n';
    }
    output.close();
    if (!output) {
        throw std::runtime_error("Failed while writing results CSV: " + results_path.string());
    }

    const std::filesystem::path distributions = distribution_path(results_path);
    std::ofstream detail(distributions);
    if (!detail) {
        throw std::runtime_error(
            "Could not open round distribution CSV: " + distributions.string()
        );
    }
    detail << "strategy,parameters,target_score,distribution,value,count\n";
    for (const SimulationResult& result : results) {
        for (const auto& [score, count] : result.round_score_distribution) {
            detail << quote_csv(result.strategy_name) << ','
                   << quote_csv(result.strategy_parameters) << ','
                   << result.target_score << ",round_score,"
                   << score << ',' << count << '\n';
        }
        for (std::size_t index = 0;
             index < result.round_end_distribution.size();
             ++index) {
            detail << quote_csv(result.strategy_name) << ','
                   << quote_csv(result.strategy_parameters) << ','
                   << result.target_score << ",round_end,"
                   << round_end_name(index) << ','
                   << result.round_end_distribution[index] << '\n';
        }
        for (const auto& [score, count] : result.final_score_distribution) {
            detail << quote_csv(result.strategy_name) << ','
                   << quote_csv(result.strategy_parameters) << ','
                   << result.target_score << ",final_score,"
                   << score << ',' << count << '\n';
        }
    }
    detail.close();
    if (!detail) {
        throw std::runtime_error(
            "Failed while writing round distribution CSV: " + distributions.string()
        );
    }
}

} // namespace flip7::experiment
