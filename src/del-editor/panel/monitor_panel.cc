#include "monitor_panel.h"

#include "imgui.h"

namespace del_editor {

void MonitorPanel::Render(DockID dockspace_id, std::string const& window_title) {
  if (!open) return;
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

void MonitorPanel::RenderContent() {
  // ---- performance section ----
  ImGui::TextDisabled("Performance");
  ImGui::Separator();

  if (metrics_.compile_us > 0 || metrics_.execute_us > 0) {
    ImGui::Text("  Compile  %7lld us  (%d expressions)", metrics_.compile_us, metrics_.expr_count);
    ImGui::Text("  Execute  %7lld us", metrics_.execute_us);
    long long total = metrics_.compile_us + metrics_.execute_us;
    ImGui::Text("  Total    %7lld us  (%.2f ms)", total, static_cast<double>(total) / 1000.0);
  } else {
    ImGui::TextDisabled("  No data yet.");
  }

  ImGui::Spacing();

  // ---- error history section ----
  ImGui::TextDisabled("Error history");
  ImGui::SameLine();
  if (ImGui::SmallButton("Clear")) {
    ClearErrors();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%zu message(s)", errors_.size());
  ImGui::Separator();

  if (ImGui::BeginChild(
          "##MonitorErrorList",
          ImVec2(0, 0),
          ImGuiChildFlags_None,
          ImGuiWindowFlags_HorizontalScrollbar
      )) {
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(errors_.size()));
    while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
        auto const& msg = errors_[static_cast<size_t>(i)];
        ImGui::BulletText("%s", msg.c_str());
      }
    }
  }
  ImGui::EndChild();
}

void MonitorPanel::AddError(std::string msg) { errors_.push_back(std::move(msg)); }

void MonitorPanel::ClearErrors() { errors_.clear(); }

bool MonitorPanel::HasErrors() const { return !errors_.empty(); }

} // namespace del_editor
