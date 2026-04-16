#pragma once

#include "memory/arc.h"

#include <cmath>
#include <thread>

using namespace memory;

static inline const u32 nCores = std::thread::hardware_concurrency();
static inline const u32 nWorkers =
    std::max(1u, static_cast<u32>(std::floor(static_cast<f32>(nCores) * 0.5f)));
static inline const u32 nExplorers = nCores - nWorkers;

using Task = Arc<std::string>;
