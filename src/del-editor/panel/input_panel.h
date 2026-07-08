#pragma once

#include <functional>
#include <string>

#include "del-editor/components/code_editor.h"

namespace del_editor {

/// @brief Source JSON input panel.
/// Displays and allows editing of the source JSON document.
class InputPanel {
public:
  InputPanel();

  void Render(ImGuiID dockspace_id);

  void                      SetText(std::string const& text);
  [[nodiscard]] std::string GetText() const;

  /// Set a callback invoked after the user stops typing (debounced ~300 ms).
  void SetChangeCallback(std::function<void()> callback);

  bool open = true;

  static constexpr const char* kPanelName = "Source JSON";

private:
  CodeEditor editor_;
};

} // namespace del_editor
