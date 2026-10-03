#pragma once

#include "flip7/orchestration/simulation_job.hpp"

#include <filesystem>
#include <vector>

namespace flip7::experiment {

using orchestration::SimulationResult;

std::vector<SimulationResult> aggregate_results(
    const std::vector<SimulationResult>& batches
);

void write_csv(
    const std::filesystem::path& results_path,
    const std::vector<SimulationResult>& results
);

} // namespace flip7::experiment
