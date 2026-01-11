#pragma once
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace q {

inline std::vector<std::string_view> split_csv_line(std::string_view line, char delim = ',') {
  std::vector<std::string_view> out;
  std::size_t start = 0;
  while (start <= line.size()) {
    auto pos = line.find(delim, start);
    if (pos == std::string_view::npos) pos = line.size();
    out.emplace_back(line.substr(start, pos - start));
    start = pos + 1;
    if (pos == line.size()) break;
  }
  return out;
}

class CsvReader {
public:
  explicit CsvReader(std::string path) : ifs_(std::move(path)) {}

  bool good() const { return ifs_.good(); }

  // returns next non-empty, non-comment line
  std::optional<std::string> next_line() {
    std::string line;
    while (std::getline(ifs_, line)) {
      if (line.empty()) continue;
      if (!line.empty() && line[0] == '#') continue;
      return line;
    }
    return std::nullopt;
  }

private:
  std::ifstream ifs_;
};

} // namespace q
