#pragma once

#include <string>

#include "TextEditor.h"

namespace del_editor {

/// @brief Output panel — displays the result JSON produced by the DEL
/// engine. Read-only with a "Copy" button to copy the content.
class OutputPanel {
public:
  OutputPanel();

  void Render(ImGuiID dockspace_id);

  void SetText(std::string const& text);
  [[nodiscard]] std::string GetText() const;

  bool open = true;

  static constexpr const char* kPanelName = "Output";

private:
  TextEditor  editor_;
  std::string text_;
};

} // namespace del_editor
