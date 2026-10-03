#include "flip7/experiment/csv.hpp"
#include "flip7/orchestration/parallel_runner.hpp"
#include "flip7/strategies/strategies.hpp"

#include <charconv>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace {

constexpr std::uint64_t BATCH_SIZE = 2048;

struct StrategyConfiguration {
    std::size_t id;
    std::string name;
    std::string parameters;
    flip7::orchestration::StrategyFactory factory;
};

struct Options {
    std::uint64_t episodes{10000};
    std::uint64_t seed{1000000};
    int target_score{200};
    std::size_t workers{
        std::max(1U, std::thread::hardware_concurrency())
    };
    std::string strategy{"all"};
    std::filesystem::path output{"results.csv"};
};

template <typename Integer>
Integer parse_integer(const std::string& text, const char* name) {
    Integer value{};
    const char* first = text.data();
    const char* last = first + text.size();
    const auto parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + text);
    }
    return value;
}

void print_usage(std::ostream& output) {
    output
        << "Usage: flip7-sweep [options]\n"
        << "  --episodes N       Episodes per strategy configuration (default: 10000)\n"
        << "  --seed N           First episode seed (default: 1000000)\n"
        << "  --target-score N   Solo score-chase target (default: 200)\n"
        << "  --threads N        Worker count (default: hardware concurrency)\n"
        << "  --strategy NAME    all, card_count, card_sum, bust_probability,\n"
        << "                     unique_number, or hybrid (default: all)\n"
        << "  --output PATH      Results CSV path (default: results.csv)\n"
        << "  --help             Show this message\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") {
            print_usage(std::cout);
            std::exit(0);
        }
        if (index + 1 >= argc) {
            throw std::invalid_argument("Missing value for option: " + argument);
        }
        const std::string value = argv[++index];
        if (argument == "--episodes") {
            options.episodes = parse_integer<std::uint64_t>(value, "episode count");
            if (options.episodes == 0) {
                throw std::invalid_argument("Episode count must be positive");
            }
        } else if (argument == "--seed") {
            options.seed = parse_integer<std::uint64_t>(value, "seed");
        } else if (argument == "--target-score") {
            options.target_score = parse_integer<int>(value, "target score");
            if (options.target_score <= 0) {
                throw std::invalid_argument("Target score must be positive");
            }
        } else if (argument == "--threads") {
            options.workers = parse_integer<std::size_t>(value, "thread count");
            if (options.workers == 0) {
                throw std::invalid_argument("Thread count must be positive");
            }
        } else if (argument == "--strategy") {
            options.strategy = value;
        } else if (argument == "--output") {
            options.output = value;
        } else {
            throw std::invalid_argument("Unknown option: " + argument);
        }
    }
    if (options.seed > UINT64_MAX - (options.episodes - 1)) {
        throw std::invalid_argument("Seed range overflows uint64_t");
    }
    return options;
}

std::vector<StrategyConfiguration> make_configurations(
    const std::string& selection
) {
    std::vector<StrategyConfiguration> configurations;
    auto add = [&](std::string name,
                   std::string parameters,
                   flip7::orchestration::StrategyFactory factory) {
        configurations.push_back({
            configurations.size(),
            std::move(name),
            std::move(parameters),
            std::move(factory)
        });
    };

    const auto selected = [&](std::string_view name) {
        return selection == "all" || selection == name;
    };

    if (selected("card_count")) {
        for (std::size_t target = 3; target <= 5; ++target) {
            add("card_count", "target_cards=" + std::to_string(target),
                [target] {
                    return std::make_unique<flip7::CardCountStrategy>(target);
                });
        }
    }
    if (selected("card_sum")) {
        for (int target = 22; target <= 31; ++target) {
            add("card_sum", "target_score=" + std::to_string(target),
                [target] {
                    return std::make_unique<flip7::CardSumStrategy>(target);
                });
        }
    }
    if (selected("bust_probability")) {
        for (int percentage = 20; percentage <= 24; ++percentage) {
            const double threshold = percentage / 100.0;
            add("bust_probability",
                "max_probability=" + std::to_string(threshold),
                [threshold] {
                    return std::make_unique<flip7::BustProbabilityStrategy>(threshold);
                });
        }
    }
    if (selected("unique_number")) {
        for (std::size_t target = 3; target <= 4; ++target) {
            add("unique_number", "target_unique=" + std::to_string(target),
                [target] {
                    return std::make_unique<flip7::UniqueNumberStrategy>(target);
                });
        }
    }
    if (selected("hybrid")) {
        for (int target_score = 23; target_score <= 28; ++target_score) {
            for (std::size_t target_unique = 3; target_unique <= 7; ++target_unique) {
                add(
                    "hybrid",
                    "target_score=" + std::to_string(target_score) +
                        ";target_unique=" + std::to_string(target_unique),
                    [target_score, target_unique] {
                        return std::make_unique<flip7::HybridStrategy>(
                            target_score,
                            target_unique
                        );
                    }
                );
            }
        }
    }

    if (configurations.empty()) {
        throw std::invalid_argument("Unknown strategy selection: " + selection);
    }
    return configurations;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const auto configurations = make_configurations(options.strategy);

        std::vector<flip7::orchestration::SimulationJob> jobs;
        for (const auto& configuration : configurations) {
            for (std::uint64_t offset = 0; offset < options.episodes;) {
                const std::uint64_t batch_size =
                    std::min(BATCH_SIZE, options.episodes - offset);
                flip7::orchestration::SimulationJob job;
                job.job_id = configuration.id;
                job.strategy_name = configuration.name;
                job.strategy_parameters = configuration.parameters;
                job.seed_start = options.seed + offset;
                job.episode_count = batch_size;
                job.target_score = options.target_score;
                job.strategy_factory = configuration.factory;
                jobs.push_back(std::move(job));
                offset += batch_size;
            }
        }

        flip7::orchestration::ParallelRunner runner(options.workers);
        const auto batches = runner.run(jobs);
        const auto results = flip7::experiment::aggregate_results(batches);
        flip7::experiment::write_csv(options.output, results);

        std::cout << "Evaluated " << configurations.size()
                  << " configurations × " << options.episodes
                  << " episodes using " << options.workers << " workers.\n"
                  << "Results: " << options.output << '\n';
        std::filesystem::path distribution_path = options.output;
        distribution_path.replace_filename(
            options.output.stem().string() + "_round_distribution.csv"
        );
        std::cout << "Distributions: " << distribution_path << '\n';
    } catch (const std::exception& error) {
        std::cerr << "flip7-sweep: " << error.what() << '\n';
        print_usage(std::cerr);
        return 2;
    }
}
