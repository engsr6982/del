#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "del/template_engine.h"

#include "del-editor/dock_id.h"
#include "del-editor/panel/editor_panel.h"
#include "del-editor/panel/input_panel.h"
#include "del-editor/panel/monitor_panel.h"
#include "del-editor/panel/output_panel.h"

namespace del_editor {

/// @brief Callbacks provided by the host application for platform-specific
/// functionality (e.g. native file dialogs).
/// - `open_file`: returns the selected file path, or empty on cancel.
///   The `filter` parameter is a hint, e.g. "json".
struct EditorHostCallbacks {
  std::function<std::string(const char* filter)> open_file;
};

/// @brief Embedding configuration. The defaults reproduce the standalone
/// editor behaviour.
///
/// The editor is host-agnostic: it never touches the ImGui context's fonts,
/// style or ini file — those belong to the host. It only creates windows,
/// so it is safe to embed into any host ImGui context, with or without
/// docking enabled (docking-disabled hosts get a stacked fallback layout).
struct EditorConfig {
  /// Window name. Must be unique within the host's ImGui context.
  std::string window_name = "DEL Live Editor";

  /// Optional suffix for panel window titles. Empty by default — panels
  /// keep short readable titles ("Source JSON", "Template", ...). Hosts
  /// that embed multiple editor instances, or already have windows with
  /// the same names, set this to keep window names unique within their
  /// ImGui context (window names are global per context).
  /// E.g. " (DEL)" → "Template (DEL)".
  std::string panel_title_suffix;

  /// Render a wrapper window (toolbar + internal dockspace) around the
  /// panels. When false the panels are rendered as top-level windows docked
  /// directly into the host's dockspace (`host_dockspace_id`) and the
  /// toolbar is omitted — the host provides its own chrome.
  bool own_window = true;

  /// When own_window is true: allow the wrapper window itself to be docked
  /// into the host's layout (omit ImGuiWindowFlags_NoDocking). The default
  /// (NoDocking) keeps the editor self-contained and avoids flicker when
  /// the host also uses docking.
  bool dockable = false;

  /// When own_window is false: the host dockspace the panels dock into on
  /// first use. 0 (or a host without docking) degrades to plain floating
  /// windows.
  DockID host_dockspace_id = 0;

  /// Initial wrapper window size (own_window only).
  float initial_width  = 1280.0f;
  float initial_height = 800.0f;
};

/// @brief The main editor instance.
///
/// In its default configuration it renders a top-level window containing a
/// toolbar and a dockspace. All panels (Input, Template, Output, Monitor)
/// live inside this window and can be rearranged via docking *within* the
/// editor instance only.
///
/// Embedding modes (see EditorConfig):
/// - own_window = true (default): self-contained window; set `dockable` to
///   allow the host to dock the whole editor into its own layout.
/// - own_window = false: panels dock straight into the host's dockspace
///   (`host_dockspace_id`), fully merging into the host's layout.
/// - Host without ImGuiConfigFlags_DockingEnable: a stacked fallback layout
///   is rendered instead of a dockspace.
///
/// The class is backend-agnostic — it only calls standard ImGui APIs.
class EditorBootstrap {
public:
  /// Default window name, exposed so embedders can reference it
  /// (e.g. for docking the editor window into a parent dockspace).
  static constexpr const char* kWindowName = "DEL Live Editor";

  explicit EditorBootstrap(EditorConfig config = {});
  ~EditorBootstrap() = default;

  EditorBootstrap(EditorBootstrap const&)            = delete;
  EditorBootstrap& operator=(EditorBootstrap const&) = delete;

  /// Attach host callbacks (native file dialogs, etc.).
  void SetHostCallbacks(EditorHostCallbacks cb) { callbacks_ = std::move(cb); }

  /// Render the entire editor. Call once per frame inside an ImGui context.
  void Render();

  /// Programmatically load source / template content.
  void LoadSourceText(std::string const& text);
  void LoadTemplateText(std::string const& text);

  /// Returns false when the user closes the editor window.
  /// Use SetOpen(true) to reopen it (e.g. from a host menu toggle).
  [[nodiscard]] bool IsOpen() const { return open_; }
  void               SetOpen(bool open) { open_ = open; }

  /// Toggle a panel by its logical name (InputPanel::kPanelName,
  /// EditorPanel::kPanelName, OutputPanel::kPanelName, MonitorPanel::kPanelName).
  /// Also reopens panels the user closed via their close button.
  void SetPanelOpen(std::string_view logical_name, bool open);

  /// Latest rendered output (empty until the first successful run).
  [[nodiscard]] std::string const& GetOutputText() const { return output_panel_.GetText(); }

  /// Invoked after every compile+execute cycle (success or error).
  void SetCompileCallback(std::function<void()> callback) { compile_callback_ = std::move(callback); }

  /// The configured window name (== config.window_name).
  [[nodiscard]] std::string const& windowName() const { return config_.window_name; }

  /// Set/update the host dockspace the panels dock into (own_window=false).
  /// Useful when the host creates its dockspace lazily (e.g. after the
  /// editor was constructed); call once per frame until it reports non-zero.
  void                 SetHostDockspace(DockID id) { config_.host_dockspace_id = id; }
  [[nodiscard]] DockID hostDockspace() const { return config_.host_dockspace_id; }

private:
  void RenderToolbar();
  void RenderPanels(DockID dockspace_id);
  void RenderPanelsFlat();
  void CompileAndExecute();
  void RunPipeline();

  // --- state ---
  EditorConfig config_;
  bool         open_ = true;

  // Panel window titles: "<Panel>" + panel_title_suffix (default: short
  // names). Stored at construction; the host keeps them unique if it
  // embeds multiple instances.
  std::string input_title_;
  std::string editor_title_;
  std::string output_title_;
  std::string monitor_title_;

  // Host callbacks
  EditorHostCallbacks   callbacks_;
  std::function<void()> compile_callback_;

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
