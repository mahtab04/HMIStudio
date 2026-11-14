#pragma once

#include "core/model/Project.h"

#include <memory>

namespace hmi
{

// A small filling-station project to edit until projects can be opened
// (phase 4). Built from the basic shapes only.
[[nodiscard]] std::unique_ptr<Project> makeSampleProject();

} // namespace hmi
