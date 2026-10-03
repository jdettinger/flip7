#pragma once

#include "flip7/orchestration/simulation_job.hpp"

#include <cstddef>
#include <vector>

namespace flip7::orchestration {

class ParallelRunner {
public:
    explicit ParallelRunner(std::size_t worker_count);

    std::vector<SimulationResult> run(
        const std::vector<SimulationJob>& jobs
    ) const;

private:
    std::size_t worker_count_;
};

} // namespace flip7::orchestration
