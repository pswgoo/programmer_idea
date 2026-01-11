#pragma once
#include <chrono>

namespace q {

using SteadyClock = std::chrono::steady_clock;
using TimePoint   = SteadyClock::time_point;
using Ns          = std::chrono::nanoseconds;

inline TimePoint now() { return SteadyClock::now(); }

} // namespace q
