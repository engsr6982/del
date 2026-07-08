#pragma once

#include <functional>
#include <string>

#include "del-editor/components/code_editor.h"

namespace del_editor {

/// @brief Template editor panel.
/// The user edits the DEL template (ordered JSON with JSON Pointer keys
/// and DEL expression values). Uses ImGuiColorTextEdit with JSON syntax
/// highlighting.
class EditorPanel {
public:
  EditorPanel();

  void Render(ImGuiID dockspace_id);

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
