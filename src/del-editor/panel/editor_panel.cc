#include "editor_panel.h"

#include "imgui.h"

namespace del_editor {

EditorPanel::EditorPanel() {
  editor_.getEditor().SetLanguage(TextEditor::Language::Json());
  editor_.getEditor().SetShowLineNumbersEnabled(true);
}

void EditorPanel::Render(DockID dockspace_id, std::string const& window_title) {
  if (!open) return;
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

void EditorPanel::RenderContent() { editor_.render(); }

void EditorPanel::SetText(std::string const& text) { editor_.getEditor().SetText(text); }

std::string EditorPanel::GetText() const { return editor_.getEditor().GetText(); }

void EditorPanel::SetChangeCallback(std::function<void()> callback) {
  editor_.getEditor().SetChangeCallback(std::move(callback), 300);
}

} // namespace del_editor
