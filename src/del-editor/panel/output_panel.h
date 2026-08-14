#pragma once

#include <string>

#include "TextEditor.h"

#include "del-editor/dock_id.h"

namespace del_editor {

/// @brief Output panel — displays the result JSON produced by the DEL
/// engine. Read-only with a "Copy" button to copy the content.
class OutputPanel {
public:
  OutputPanel();

  /// Render the panel as a window docked into `dockspace_id`
  /// (0 = plain floating window, no docking request). The window title is
  /// supplied by the host (EditorBootstrap namespaces it per instance).
  void Render(DockID dockspace_id, std::string const& window_title);

  /// Render the panel content into the current window (no Begin/End).
  /// Used by the flat fallback layout when the host has no docking.
  void RenderContent();

  void                             SetText(std::string const& text);
  [[nodiscard]] std::string const& GetText() const { return text_; }

  bool open = true;

  static constexpr const char* kPanelName = "Output";

private:
  TextEditor  editor_;
  std::string text_;
};

} // namespace del_editor
