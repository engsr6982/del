#pragma once

#include "imgui.h"

#include <chrono>
#include <string>
#include <vector>

namespace del_editor {

/// @brief Performance metrics for the last compile+execute cycle.
struct PerfMetrics {
  long long compile_us  = 0;  // template compilation time (microseconds)
  long long execute_us  = 0;  // execution time (microseconds)
  int       expr_count  = 0;  // number of expressions in the template
};

/// @brief Monitor panel — displays performance metrics (compile / execute
/// timing) and error history. Errors are stored as raw strings (typically
/// exception::what()) and accumulate until the user clears them.
class MonitorPanel {
public:
  MonitorPanel() = default;

  void Render(ImGuiID dockspace_id);

  /// Set the latest performance snapshot.
  void SetMetrics(PerfMetrics const& m) { metrics_ = m; }

  /// Append an error message (usually exception::what()).
  void AddError(std::string msg);

  void ClearErrors();
  [[nodiscard]] bool HasErrors() const;

  bool open = true;

  static constexpr const char* kPanelName = "Monitor";

private:
  PerfMetrics          metrics_;
  std::vector<std::string> errors_;
};

} // namespace del_editor
