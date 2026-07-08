#include "editor_panel.h"

#include "imgui.h"

namespace del_editor {

EditorPanel::EditorPanel() {
  editor_.getEditor().SetLanguage(TextEditor::Language::Json());
  editor_.getEditor().SetShowLineNumbersEnabled(true);
}

void EditorPanel::Render(ImGuiID dockspace_id) {
  ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(kPanelName, &open, ImGuiWindowFlags_MenuBar)) {
    ImGui::End();
    return;
  }

  editor_.render();
  ImGui::End();
}

void EditorPanel::SetText(std::string const& text) { editor_.getEditor().SetText(text); }

std::string EditorPanel::GetText() const { return editor_.getEditor().GetText(); }

void EditorPanel::SetChangeCallback(std::function<void()> callback) {
  editor_.getEditor().SetChangeCallback(std::move(callback), 300);
}

} // namespace del_editor
