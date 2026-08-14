#include "input_panel.h"

#include "imgui.h"

namespace del_editor {

InputPanel::InputPanel() {
  editor_.getEditor().SetLanguage(TextEditor::Language::Json());
  editor_.getEditor().SetShowLineNumbersEnabled(true);
  editor_.showStatusBar = false;
}

void InputPanel::Render(DockID dockspace_id, std::string const& window_title) {
  if (dockspace_id != 0) {
    ImGui::SetNextWindowDockID(static_cast<ImGuiID>(dockspace_id), ImGuiCond_FirstUseEver);
  }
  if (!ImGui::Begin(window_title.c_str(), &open, ImGuiWindowFlags_MenuBar)) {
    ImGui::End();
    return;
  }

  RenderContent();
  ImGui::End();
}

void InputPanel::RenderContent() { editor_.render(); }

void InputPanel::SetText(std::string const& text) { editor_.getEditor().SetText(text); }

std::string InputPanel::GetText() const { return editor_.getEditor().GetText(); }

void InputPanel::SetChangeCallback(std::function<void()> callback) {
  editor_.getEditor().SetChangeCallback(std::move(callback), 300);
}

} // namespace del_editor
