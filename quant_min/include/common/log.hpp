#pragma once
#include <chrono>
#include <cstdio>
#include <string_view>

namespace q::log {

inline void info(std::string_view msg) {
  std::fprintf(stdout, "[INFO] %.*s\n", (int)msg.size(), msg.data());
}

inline void warn(std::string_view msg) {
  std::fprintf(stderr, "[WARN] %.*s\n", (int)msg.size(), msg.data());
}

} // namespace q::log
