#include "output_panel.h"

#include "imgui.h"

namespace del_editor {

OutputPanel::OutputPanel() {
  editor_.SetLanguage(TextEditor::Language::Json());
  editor_.SetReadOnlyEnabled(true);
  editor_.SetShowLineNumbersEnabled(true);
}

void OutputPanel::Render(DockID dockspace_id, std::string const& window_title) {
  if (dockspace_id != 0) {
    ImGui::SetNextWindowDockID(static_cast<ImGuiID>(dockspace_id), ImGuiCond_FirstUseEver);
  }
  if (!ImGui::Begin(window_title.c_str(), &open)) {
    ImGui::End();
    return;
  }

  RenderContent();
  ImGui::End();
}

void OutputPanel::RenderContent() {
  // Copy button in the panel header area
  if (ImGui::Button("Copy to clipboard")) {
    ImGui::SetClipboardText(text_.c_str());
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(read-only)");

  ImGui::Separator();

  editor_.Render("##OutputEditor", ImVec2(-1.0f, -1.0f), false);
}

void OutputPanel::SetText(std::string const& text) {
  text_ = text;
  editor_.SetText(text_);
}

} // namespace del_editor
