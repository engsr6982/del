#pragma once

#include <functional>
#include <string>

#include "del-editor/components/code_editor.h"
#include "del-editor/dock_id.h"

namespace del_editor {

/// @brief Template editor panel.
/// The user edits the DEL template (ordered JSON with JSON Pointer keys
/// and DEL expression values). Uses ImGuiColorTextEdit with JSON syntax
/// highlighting.
class EditorPanel {
public:
  EditorPanel();

  /// Render the panel as a window docked into `dockspace_id`
  /// (0 = plain floating window, no docking request). The window title is
  /// supplied by the host (EditorBootstrap namespaces it per instance).
  void Render(DockID dockspace_id, std::string const& window_title);

  /// Render the panel content into the current window (no Begin/End).
  /// Used by the flat fallback layout when the host has no docking.
  void RenderContent();

  void                      SetText(std::string const& text);
  [[nodiscard]] std::string GetText() const;

  /// Set a callback invoked after the user stops typing (debounced ~300 ms).
  void SetChangeCallback(std::function<void()> callback);

  bool open = true;

  static constexpr const char* kPanelName = "Template";

private:
  CodeEditor editor_;
};

} // namespace del_editor
