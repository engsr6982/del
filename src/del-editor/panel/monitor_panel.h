#pragma once

#include <string>
#include <vector>

#include "del-editor/dock_id.h"

namespace del_editor {

/// @brief Performance metrics for the last compile+execute cycle.
struct PerfMetrics {
  long long compile_us = 0; // template compilation time (microseconds)
  long long execute_us = 0; // execution time (microseconds)
  int       expr_count = 0; // number of expressions in the template
};

/// @brief Monitor panel — displays performance metrics (compile / execute
/// timing) and error history. Errors are stored as raw strings (typically
/// exception::what()) and accumulate until the user clears them.
class MonitorPanel {
public:
  MonitorPanel() = default;

  /// Render the panel as a window docked into `dockspace_id`
  /// (0 = plain floating window, no docking request). The window title is
  /// supplied by the host (EditorBootstrap namespaces it per instance).
  void Render(DockID dockspace_id, std::string const& window_title);

  /// Render the panel content into the current window (no Begin/End).
  /// Used by the flat fallback layout when the host has no docking.
  void RenderContent();

  /// Set the latest performance snapshot.
  void SetMetrics(PerfMetrics const& m) { metrics_ = m; }

  /// Append an error message (usually exception::what()).
  void AddError(std::string msg);

  void               ClearErrors();
  [[nodiscard]] bool HasErrors() const;

  bool open = true;

  static constexpr const char* kPanelName = "Monitor";

private:
  PerfMetrics              metrics_;
  std::vector<std::string> errors_;
};

} // namespace del_editor
