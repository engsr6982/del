#include "output_panel.h"

#include "imgui.h"

namespace del_editor {

OutputPanel::OutputPanel() {
  editor_.SetLanguage(TextEditor::Language::Json());
  editor_.SetReadOnlyEnabled(true);
  editor_.SetShowLineNumbersEnabled(true);
}

void OutputPanel::Render(ImGuiID dockspace_id) {
  ImGui::SetNextWindowDockID(dockspace_id, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(kPanelName, &open)) {
    ImGui::End();
    return;
  }

  // Copy button in the panel header area
  if (ImGui::Button("Copy to clipboard")) {
    ImGui::SetClipboardText(text_.c_str());
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(read-only)");

  ImGui::Separator();

  editor_.Render("##OutputEditor", ImVec2(-1.0f, -1.0f), false);
  ImGui::End();
}

void OutputPanel::SetText(std::string const& text) {
  text_ = text;
  editor_.SetText(text_);
}

std::string OutputPanel::GetText() const { return text_; }

} // namespace del_editor
