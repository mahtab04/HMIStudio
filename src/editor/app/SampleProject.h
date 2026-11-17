#pragma once

#include "core/model/Project.h"

#include <memory>

namespace hmi
{

// Operable buffer-filling training project with an explicitly saved process simulator.
[[nodiscard]] std::unique_ptr<Project> makeSampleProject();

} // namespace hmi
