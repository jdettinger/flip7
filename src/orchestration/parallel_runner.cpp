#include "flip7/orchestration/parallel_runner.hpp"

#include "flip7/simulation.hpp"

#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace flip7::orchestration {
namespace {

SimulationResult run_job(const SimulationJob& job) {
    if (!job.strategy_factory) {
        throw std::invalid_argument("SimulationJob has no strategy factory");
    }
    if (job.episode_count == 0) {
        throw std::invalid_argument("SimulationJob must contain at least one episode");
    }
    if (job.target_score <= 0) {
        throw std::invalid_argument("SimulationJob target score must be positive");
    }
    if (job.seed_start > UINT64_MAX - (job.episode_count - 1)) {
        throw std::overflow_error("SimulationJob seed range overflows uint64_t");
    }

    SimulationResult output;
    output.job_id = job.job_id;
    output.strategy_name = job.strategy_name;
    output.strategy_parameters = job.strategy_parameters;
    output.seed_start = job.seed_start;
    output.target_score = job.target_score;

    for (std::uint64_t offset = 0; offset < job.episode_count; ++offset) {
        auto strategy = job.strategy_factory();
        if (!strategy) {
            throw std::runtime_error("Strategy factory returned null");
        }
        const EpisodeResult episode = run_episode(
            job.seed_start + offset,
            job.target_score,
            *strategy
        );

        ++output.episode_count;
        output.rounds_played += episode.rounds;
        output.actions += episode.actions;
        output.cards_drawn += episode.cards_drawn;
        output.busts += episode.busts;
        output.flip7s += episode.flip7s;
        output.voluntary_finishes += episode.voluntary_finishes;
        output.target_reached_episodes += episode.target_reached ? 1U : 0U;
        output.deck_exhausted_episodes +=
            episode.end == GameEnd::DeckExhausted ? 1U : 0U;
        output.final_score_sum += episode.final_score;
        output.final_score_square_sum +=
            static_cast<long double>(episode.final_score) * episode.final_score;
        ++output.final_score_distribution[episode.final_score];
        for (const RoundResult& round : episode.round_results) {
            ++output.round_score_distribution[round.score];
            ++output.round_end_distribution[static_cast<std::size_t>(round.end)];
        }
    }
    return output;
}

} // namespace

ParallelRunner::ParallelRunner(std::size_t worker_count)
    : worker_count_(std::max<std::size_t>(1, worker_count)) {}

std::vector<SimulationResult> ParallelRunner::run(
    const std::vector<SimulationJob>& jobs
) const {
    if (jobs.empty()) {
        return {};
    }

    const std::size_t workers = std::min(worker_count_, jobs.size());
    std::vector<SimulationResult> results(jobs.size());
    std::atomic<std::size_t> next_job{0};
    std::exception_ptr first_exception;
    std::mutex exception_mutex;
    std::vector<std::thread> threads;
    threads.reserve(workers);

    try {
        for (std::size_t worker = 0; worker < workers; ++worker) {
            threads.emplace_back([&]() {
                while (true) {
                    const std::size_t index =
                        next_job.fetch_add(1, std::memory_order_relaxed);
                    if (index >= jobs.size()) {
                        break;
                    }
                    try {
                        results[index] = run_job(jobs[index]);
                    } catch (...) {
                        std::lock_guard<std::mutex> lock(exception_mutex);
                        if (!first_exception) {
                            first_exception = std::current_exception();
                        }
                    }
                }
            });
        }
    } catch (...) {
        for (auto& thread : threads) {
            thread.join();
        }
        throw;
    }

    for (auto& thread : threads) {
        thread.join();
    }
    if (first_exception) {
        std::rethrow_exception(first_exception);
    }
    return results;
}

} // namespace flip7::orchestration
