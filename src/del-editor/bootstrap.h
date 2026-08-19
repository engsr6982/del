#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "del-editor/dock_id.h"
#include "del-editor/panel/editor_panel.h"
#include "del-editor/panel/input_panel.h"
#include "del-editor/panel/monitor_panel.h"
#include "del-editor/panel/output_panel.h"

namespace del {
class TemplateEngine;
}

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
/// docking enabled.
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

  /// Whether the editor renders its own top-level window (toolbar +
  /// internal dockspace + panels). false renders the bare panels instead,
  /// docked directly into the host's dockspace (the host provides its own
  /// chrome; only for hosts that want full layout ownership).
  bool own_window = true;

  /// When own_window is true: fill the whole viewport (menu bar +
  /// fullscreen dockspace, no wrapper window). Set false for a regular
  /// dockable window — it carries the toolbar and an internal dockspace,
  /// and docks into the host's layout on first use (`host_dockspace_id`).
  /// Embedding hosts should use own_window = true, fullscreen = false so
  /// the data-loading / compile toolbar stays reachable.
  bool fullscreen = true;

  /// When own_window is true and fullscreen is false: the host dockspace
  /// the editor window docks into on first use. 0 (or a host without
  /// docking) leaves the window floating.
  DockID host_dockspace_id = 0;

  /// Initial editor window size (own_window + !fullscreen only; used while
  /// floating — once docked the host layout controls the size).
  float initial_width  = 900.0f;
  float initial_height = 600.0f;
};

/// @brief The main editor instance.
///
/// Modes (see EditorConfig):
/// - own_window + fullscreen (default): standalone — main menu bar +
///   fullscreen dockspace over the whole viewport, no wrapper window.
/// - own_window, !fullscreen: embeddable dockable window — toolbar +
///   internal dockspace + panels; docks into the host's layout on first
///   use (host_dockspace_id). This is the recommended embedding mode: the
///   toolbar (load source/template, compile, panel toggles) stays usable
///   and the panels stay grouped inside the editor window.
/// - !own_window: bare panels docked straight into the host's dockspace
///   (no toolbar — host provides its own chrome).
/// - Host without ImGuiConfigFlags_DockingEnable: a stacked fallback layout
///   is rendered instead of a dockspace.
///
/// The class is backend-agnostic — it only calls standard ImGui APIs.
class EditorBootstrap {
public:
  /// Default window name, exposed so embedders can reference it
  /// (e.g. for docking the editor window into a parent dockspace).
  static constexpr const char* kWindowName = "DEL Live Editor";

  /// Creates the editor with an internally-built default DEL engine.
  explicit EditorBootstrap(EditorConfig config = {});

  /// Takes ownership of a host-prepared DEL engine (e.g. one with custom
  /// functions registered). A null engine falls back to a default instance.
  explicit EditorBootstrap(std::unique_ptr<del::TemplateEngine> engine, EditorConfig config = {});

  ~EditorBootstrap();

  EditorBootstrap(EditorBootstrap const&)            = delete;
  EditorBootstrap& operator=(EditorBootstrap const&) = delete;

  /// Attach host callbacks (native file dialogs, etc.).
  void SetHostCallbacks(EditorHostCallbacks cb) { callbacks_ = std::move(cb); }

  /// Replace the internal DEL engine with one the host has prepared
  /// (e.g. an instance with custom functions registered). The editor takes
  /// ownership of the passed engine. Call before the first Render() that
  /// compiles/executes anything.
  void SetTemplateEngine(std::unique_ptr<del::TemplateEngine> engine);

  /// Render the entire editor. Call once per frame inside an ImGui context.
  void Render();

  /// Programmatically load source / template content.
  void LoadSourceText(std::string const& text);
  void LoadTemplateText(std::string const& text);

  /// Whether the editor should render. Fullscreen mode has no close
  /// button, so this stays true unless the host calls SetOpen(false).
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
  void RenderToolbar(bool main_menu_bar);
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

  // DEL engine (owned; default instance unless one is injected via ctor)
  std::unique_ptr<del::TemplateEngine> engine_;

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
