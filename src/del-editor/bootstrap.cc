#include "bootstrap.h"

#include <fstream>
#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"

#include "del/ast.h"
#include "del/exception.h"

#include <nlohmann/json.hpp>

namespace del_editor {

// --- helpers ---------------------------------------------------------------

static std::string ReadFileToString(char const* path) {
  if (!path || path[0] == '\0') return {};
  std::ifstream     fin(path, std::ios::binary);
  std::stringstream buf;
  buf << fin.rdbuf();
  if (!fin) return {};
  return buf.str();
}

static std::string JsonToPrettyString(nlohmann::json const& j) {
  return j.dump(2); // 2-space indent
}

// --- EditorBootstrap --------------------------------------------------------

EditorBootstrap::EditorBootstrap() {
  auto mark_dirty = [this]() { dirty_ = true; };
  input_panel_.SetChangeCallback(mark_dirty);
  editor_panel_.SetChangeCallback(mark_dirty);
}

void EditorBootstrap::Render() {
  if (!open_) return;

  // --- main editor window ---
  // NoDocking prevents the editor window itself from being docked into a
  // parent dockspace, avoiding flickering / deadlock when the host also
  // uses docking with this window.
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_NoCollapse;

  ImGui::SetNextWindowSize(ImVec2(1280, 800), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin(kWindowName, &open_, window_flags)) {
    ImGui::End();
    return;
  }

  // --- toolbar ---
  RenderToolbar();

  // --- central dockspace (internal docking only) ---
  ImGuiID dockspace_id = ImGui::GetID("##EditorDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

  // --- render visible panels ---
  if (show_input_)   input_panel_.Render(dockspace_id);
  if (show_editor_)  editor_panel_.Render(dockspace_id);
  if (show_output_)  output_panel_.Render(dockspace_id);
  if (show_monitor_) monitor_panel_.Render(dockspace_id);

  ImGui::End();

  // --- deferred recompilation ---
  if (dirty_ && live_compile_) {
    CompileAndExecute();
    dirty_ = false;
  }
}

// --- toolbar ---------------------------------------------------------------

void EditorBootstrap::RenderToolbar() {
  if (!ImGui::BeginMenuBar()) return;

  // ---- file loading ----
  // Source JSON
  ImGui::SetNextItemWidth(180);
  ImGui::InputTextWithHint("##SourcePath", "source.json", source_path_,
                           sizeof(source_path_));
  ImGui::SameLine();
  if (ImGui::Button("Load Source")) {
    std::string content = ReadFileToString(source_path_);
    if (!content.empty()) LoadSourceText(content);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("...##BrowseSource")) {
    if (callbacks_.open_file) {
      std::string path = callbacks_.open_file("json");
      if (!path.empty()) {
        size_t n = path.copy(source_path_, sizeof(source_path_) - 1);
        source_path_[n] = '\0';
        std::string content = ReadFileToString(source_path_);
        if (!content.empty()) LoadSourceText(content);
      }
    }
  }

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // Template JSON
  ImGui::SetNextItemWidth(180);
  ImGui::InputTextWithHint("##TemplatePath", "template.json", template_path_,
                           sizeof(template_path_));
  ImGui::SameLine();
  if (ImGui::Button("Load Template")) {
    std::string content = ReadFileToString(template_path_);
    if (!content.empty()) LoadTemplateText(content);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("...##BrowseTemplate")) {
    if (callbacks_.open_file) {
      std::string path = callbacks_.open_file("json");
      if (!path.empty()) {
        size_t n = path.copy(template_path_, sizeof(template_path_) - 1);
        template_path_[n] = '\0';
        std::string content = ReadFileToString(template_path_);
        if (!content.empty()) LoadTemplateText(content);
      }
    }
  }

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // ---- live compile toggle + manual compile button ----
  ImGui::Checkbox("Live", &live_compile_);
  ImGui::SameLine();
  if (!live_compile_) {
    if (ImGui::Button("Compile")) {
      CompileAndExecute();
      dirty_ = false;
    }
  } else {
    ImGui::TextDisabled("(auto)");
  }

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // ---- panel visibility toggles ----
  ImGui::Checkbox("Input",    &show_input_);   ImGui::SameLine();
  ImGui::Checkbox("Template", &show_editor_);  ImGui::SameLine();
  ImGui::Checkbox("Output",   &show_output_);  ImGui::SameLine();
  ImGui::Checkbox("Monitor",  &show_monitor_);

  ImGui::EndMenuBar();
}

// --- public helpers --------------------------------------------------------

void EditorBootstrap::LoadSourceText(std::string const& text) {
  input_panel_.SetText(text);
  dirty_ = true;
}

void EditorBootstrap::LoadTemplateText(std::string const& text) {
  editor_panel_.SetText(text);
  dirty_ = true;
}

// --- DEL engine pipeline ---------------------------------------------------

void EditorBootstrap::CompileAndExecute() {
  namespace chr = std::chrono;

  monitor_panel_.ClearErrors();

  std::string source_str   = input_panel_.GetText();
  std::string template_str = editor_panel_.GetText();

  if (source_str.empty() && template_str.empty()) {
    monitor_panel_.SetMetrics({});
    output_panel_.SetText("");
    return;
  }

  // --- 1. parse source JSON ---
  nlohmann::json source_json;
  if (!source_str.empty()) {
    try {
      source_json = nlohmann::json::parse(source_str);
    } catch (nlohmann::json::parse_error& e) {
      monitor_panel_.AddError(e.what());
      monitor_panel_.SetMetrics({});
      output_panel_.SetText("");
      return;
    }
  }

  // --- 2. parse template JSON ---
  if (template_str.empty()) {
    monitor_panel_.SetMetrics({});
    output_panel_.SetText("");
    return;
  }

  nlohmann::ordered_json template_json;
  try {
    template_json = nlohmann::ordered_json::parse(template_str);
  } catch (nlohmann::json::parse_error& e) {
    monitor_panel_.AddError(e.what());
    monitor_panel_.SetMetrics({});
    output_panel_.SetText("");
    return;
  }

  // --- 3. compile template (timed) ---
  PerfMetrics metrics;
  metrics.expr_count = static_cast<int>(template_json.size());

  del::CompiledTemplate compiled;
  auto t0 = chr::high_resolution_clock::now();
  try {
    compiled = engine_.Compile(template_json);
  } catch (del::SyntaxError& e) {
    monitor_panel_.AddError(e.what());
    monitor_panel_.SetMetrics({});
    output_panel_.SetText("");
    return;
  } catch (std::exception& e) {
    monitor_panel_.AddError(e.what());
    monitor_panel_.SetMetrics({});
    output_panel_.SetText("");
    return;
  }
  auto t1 = chr::high_resolution_clock::now();
  metrics.compile_us =
      chr::duration_cast<chr::microseconds>(t1 - t0).count();

  // --- 4. execute (timed) ---
  nlohmann::json result;
  try {
    result = engine_.Execute(compiled, source_json);
  } catch (del::RuntimeError& e) {
    auto t2 = chr::high_resolution_clock::now();
    metrics.execute_us =
        chr::duration_cast<chr::microseconds>(t2 - t1).count();
    monitor_panel_.AddError(e.what());
    monitor_panel_.SetMetrics(metrics);
    output_panel_.SetText("");
    return;
  } catch (std::exception& e) {
    auto t2 = chr::high_resolution_clock::now();
    metrics.execute_us =
        chr::duration_cast<chr::microseconds>(t2 - t1).count();
    monitor_panel_.AddError(e.what());
    monitor_panel_.SetMetrics(metrics);
    output_panel_.SetText("");
    return;
  }
  auto t2 = chr::high_resolution_clock::now();
  metrics.execute_us =
      chr::duration_cast<chr::microseconds>(t2 - t1).count();

  // --- 5. display ---
  monitor_panel_.SetMetrics(metrics);
  output_panel_.SetText(JsonToPrettyString(result));
}

} // namespace del_editor
