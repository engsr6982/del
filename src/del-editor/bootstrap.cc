#include "bootstrap.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <utility>

#include "imgui.h"

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

EditorBootstrap::EditorBootstrap(EditorConfig config) : config_(std::move(config)) {
  // Panel titles: short readable names by default; hosts that need unique
  // window names add a suffix via panel_title_suffix.
  input_title_   = std::string(InputPanel::kPanelName) + config_.panel_title_suffix;
  editor_title_  = std::string(EditorPanel::kPanelName) + config_.panel_title_suffix;
  output_title_  = std::string(OutputPanel::kPanelName) + config_.panel_title_suffix;
  monitor_title_ = std::string(MonitorPanel::kPanelName) + config_.panel_title_suffix;

  auto mark_dirty = [this]() { dirty_ = true; };
  input_panel_.SetChangeCallback(mark_dirty);
  editor_panel_.SetChangeCallback(mark_dirty);
}

void EditorBootstrap::Render() {
  if (!open_) return;

  bool const docking_enabled = (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0;

  if (config_.own_window) {
    // --- standalone fullscreen mode ---
    // The editor is the only window in the context, so there is no wrapper
    // window: a main menu bar plus a fullscreen dockspace fill the viewport.
    RenderToolbar();

    if (docking_enabled) {
      // ID is derived at the top of the ID stack, unique per context.
      DockID dockspace_id = ImGui::GetID("##EditorDockSpace");
      ImGui::DockSpaceOverViewport(static_cast<ImGuiID>(dockspace_id), nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
      RenderPanels(dockspace_id);
    } else {
      // Host without docking — fullscreen window with a stacked layout.
      ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                    | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
                                    | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
      ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
      ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
      if (ImGui::Begin(config_.window_name.c_str(), nullptr, window_flags)) {
        RenderPanelsFlat();
      }
      ImGui::End();
    }
  } else {
    // --- host-owned layout: panels dock into the host's dockspace ----
    // No wrapper window, no toolbar — the host provides its own chrome and
    // controls the panels through SetPanelOpen / LoadSourceText & co.
    // Panels carry no docking restrictions: they can be dragged out of the
    // editor group and docked anywhere in the host layout, and host windows
    // can dock into the panel group (plain dock nodes, ImGui defaults).
    DockID host_dockspace = (docking_enabled && config_.host_dockspace_id != 0)
                              ? config_.host_dockspace_id
                              : 0; // no docking available -> plain floating windows
    RenderPanels(host_dockspace);
  }

  // --- deferred recompilation ---
  if (dirty_ && live_compile_) {
    CompileAndExecute();
    dirty_ = false;
  }
}

// --- panels ----------------------------------------------------------------

void EditorBootstrap::RenderPanels(DockID dockspace_id) {
  if (show_input_) input_panel_.Render(dockspace_id, input_title_);
  if (show_editor_) editor_panel_.Render(dockspace_id, editor_title_);
  if (show_output_) output_panel_.Render(dockspace_id, output_title_);
  if (show_monitor_) monitor_panel_.Render(dockspace_id, monitor_title_);
}

void EditorBootstrap::RenderPanelsFlat() {
  // Fallback layout for hosts without docking: stack the visible panels
  // vertically inside the fullscreen window, each in a bordered child
  // region. The last visible panel fills the remaining height.
  int const visible = (show_input_ ? 1 : 0) + (show_editor_ ? 1 : 0) + (show_output_ ? 1 : 0) + (show_monitor_ ? 1 : 0);
  if (visible == 0) return;

  float const avail = ImGui::GetContentRegionAvail().y;
  float const slot  = avail / static_cast<float>(visible);
  int         index = 0;

  auto panel_slot = [&](bool& show, std::string const& title, auto const& render_content) {
    if (!show) return;
    ++index;
    float const height = (index == visible) ? 0.0f : slot; // 0 = fill remainder
    ImGui::BeginChild(title.c_str(), ImVec2(0.0f, height), ImGuiChildFlags_Borders);
    render_content();
    ImGui::EndChild();
  };

  panel_slot(show_input_, input_title_, [this] { input_panel_.RenderContent(); });
  panel_slot(show_editor_, editor_title_, [this] { editor_panel_.RenderContent(); });
  panel_slot(show_output_, output_title_, [this] { output_panel_.RenderContent(); });
  panel_slot(show_monitor_, monitor_title_, [this] { monitor_panel_.RenderContent(); });
}

// --- toolbar ---------------------------------------------------------------

void EditorBootstrap::RenderToolbar() {
  // 全屏模式的全局菜单栏 (own_window=true 专用; 嵌入模式不渲染工具栏)
  if (!ImGui::BeginMainMenuBar()) return;

  // ---- file loading ----
  // Source JSON
  ImGui::SetNextItemWidth(180);
  ImGui::InputTextWithHint("##SourcePath", "source.json", source_path_, sizeof(source_path_));
  ImGui::SameLine();
  if (ImGui::Button("Load Source")) {
    std::string content = ReadFileToString(source_path_);
    if (!content.empty()) LoadSourceText(content);
  }
  // 浏览按钮依赖宿主提供 open_file 回调; 未提供时只保留手动 Load
  if (callbacks_.open_file) {
    ImGui::SameLine();
    if (ImGui::SmallButton("...##BrowseSource")) {
      std::string path = callbacks_.open_file("json");
      if (!path.empty()) {
        size_t n            = path.copy(source_path_, sizeof(source_path_) - 1);
        source_path_[n]     = '\0';
        std::string content = ReadFileToString(source_path_);
        if (!content.empty()) LoadSourceText(content);
      }
    }
  }

  ImGui::SameLine();
  ImGui::Separator(); // vertical inside a menu bar (public API, no imgui_internal needed)
  ImGui::SameLine();

  // Template JSON
  ImGui::SetNextItemWidth(180);
  ImGui::InputTextWithHint("##TemplatePath", "template.json", template_path_, sizeof(template_path_));
  ImGui::SameLine();
  if (ImGui::Button("Load Template")) {
    std::string content = ReadFileToString(template_path_);
    if (!content.empty()) LoadTemplateText(content);
  }
  if (callbacks_.open_file) {
    ImGui::SameLine();
    if (ImGui::SmallButton("...##BrowseTemplate")) {
      std::string path = callbacks_.open_file("json");
      if (!path.empty()) {
        size_t n            = path.copy(template_path_, sizeof(template_path_) - 1);
        template_path_[n]   = '\0';
        std::string content = ReadFileToString(template_path_);
        if (!content.empty()) LoadTemplateText(content);
      }
    }
  }

  ImGui::SameLine();
  ImGui::Separator(); // vertical inside a menu bar (public API, no imgui_internal needed)
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
  ImGui::Separator(); // vertical inside a menu bar (public API, no imgui_internal needed)
  ImGui::SameLine();

  // ---- panel visibility toggles ----
  // Checkbox state mirrors `show && open`: closing a panel via its X button
  // unchecks it here, re-checking reopens it.
  auto toggle = [](char const* label, bool& show, bool& panel_open) {
    bool checked = show && panel_open;
    if (ImGui::Checkbox(label, &checked)) {
      show       = checked;
      panel_open = checked;
    }
    ImGui::SameLine();
  };
  toggle("Input", show_input_, input_panel_.open);
  toggle("Template", show_editor_, editor_panel_.open);
  toggle("Output", show_output_, output_panel_.open);
  toggle("Monitor", show_monitor_, monitor_panel_.open);

  ImGui::EndMainMenuBar();
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

void EditorBootstrap::SetPanelOpen(std::string_view logical_name, bool open) {
  if (logical_name == InputPanel::kPanelName) {
    input_panel_.open = open;
  } else if (logical_name == EditorPanel::kPanelName) {
    editor_panel_.open = open;
  } else if (logical_name == OutputPanel::kPanelName) {
    output_panel_.open = open;
  } else if (logical_name == MonitorPanel::kPanelName) {
    monitor_panel_.open = open;
  }
}

// --- DEL engine pipeline ---------------------------------------------------

void EditorBootstrap::CompileAndExecute() {
  RunPipeline();
  if (compile_callback_) compile_callback_();
}

void EditorBootstrap::RunPipeline() {
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
  auto                  t0 = chr::high_resolution_clock::now();
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
  auto t1            = chr::high_resolution_clock::now();
  metrics.compile_us = chr::duration_cast<chr::microseconds>(t1 - t0).count();

  // --- 4. execute (timed) ---
  nlohmann::json result;
  try {
    result = engine_.Execute(compiled, source_json);
  } catch (del::RuntimeError& e) {
    auto t2            = chr::high_resolution_clock::now();
    metrics.execute_us = chr::duration_cast<chr::microseconds>(t2 - t1).count();
    monitor_panel_.AddError(e.what());
    monitor_panel_.SetMetrics(metrics);
    output_panel_.SetText("");
    return;
  } catch (std::exception& e) {
    auto t2            = chr::high_resolution_clock::now();
    metrics.execute_us = chr::duration_cast<chr::microseconds>(t2 - t1).count();
    monitor_panel_.AddError(e.what());
    monitor_panel_.SetMetrics(metrics);
    output_panel_.SetText("");
    return;
  }
  auto t2            = chr::high_resolution_clock::now();
  metrics.execute_us = chr::duration_cast<chr::microseconds>(t2 - t1).count();

  // --- 5. display ---
  monitor_panel_.SetMetrics(metrics);
  output_panel_.SetText(JsonToPrettyString(result));
}

} // namespace del_editor
