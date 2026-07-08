#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "del/template_engine.h"

#include "panel/editor_panel.h"
#include "panel/input_panel.h"
#include "panel/monitor_panel.h"
#include "panel/output_panel.h"

namespace del_editor {

/// @brief Callbacks provided by the host application for platform-specific
/// functionality (e.g. native file dialogs).
/// - `open_file`: returns the selected file path, or empty on cancel.
///   The `filter` parameter is a hint, e.g. "json".
struct EditorHostCallbacks {
  std::function<std::string(const char* filter)> open_file;
};

/// @brief The main editor instance.
///
/// Renders a top-level window containing a toolbar and a dockspace.
/// All panels (Input, Template, Output, Monitor) live inside this window
/// and can be rearranged via docking *within* the editor instance only.
///
/// The class is backend-agnostic — it only calls standard ImGui APIs.
class EditorBootstrap {
public:
  /// Window name exposed as a constant so embedders can reference it
  /// (e.g. for docking the editor window into a parent dockspace).
  static constexpr const char* kWindowName = "DEL Live Editor";

  EditorBootstrap();
  ~EditorBootstrap() = default;

  /// Attach host callbacks (native file dialogs, etc.).
  void SetHostCallbacks(EditorHostCallbacks cb) { callbacks_ = std::move(cb); }

  /// Render the entire editor. Call once per frame inside an ImGui context.
  void Render();

  /// Programmatically load source / template content.
  void LoadSourceText(std::string const& text);
  void LoadTemplateText(std::string const& text);

  /// Returns false when the user closes the editor window.
  [[nodiscard]] bool IsOpen() const { return open_; }

private:
  void RenderToolbar();
  void CompileAndExecute();

  // --- state ---
  bool open_ = true;

  // Host callbacks
  EditorHostCallbacks callbacks_;

  // DEL engine
  del::TemplateEngine engine_;

  // Panels
  InputPanel   input_panel_;
  EditorPanel  editor_panel_;
  OutputPanel  output_panel_;
  MonitorPanel monitor_panel_;

  // Panel visibility toggles
  bool show_input_   = true;
  bool show_editor_  = true;
  bool show_output_  = true;
  bool show_monitor_ = true;

  // Live-compile toggle — when off the user must press "Compile" manually
  bool live_compile_ = true;

  // File path buffers for toolbar
  char source_path_[512]   = {};
  char template_path_[512] = {};

  // Dirty flag for live recompilation
  bool dirty_ = false;
};

} // namespace del_editor
