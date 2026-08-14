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
    // --- wrapper window (toolbar + internal dockspace / flat fallback) ----
    // NoDocking prevents the wrapper window itself from being docked into a
    // parent dockspace, avoiding flickering / deadlock when the host also
    // uses docking with this window. Set `dockable` to opt into host docking.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse;
    if (!config_.dockable) window_flags |= ImGuiWindowFlags_NoDocking;

    ImGui::SetNextWindowSize(ImVec2(config_.initial_width, config_.initial_height), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(config_.window_name.c_str(), &open_, window_flags)) {
      ImGui::End();
      return;
    }

    // --- toolbar ---
    RenderToolbar();

    if (docking_enabled) {
      // --- central dockspace (internal docking only) ---
      // The ID is derived from the wrapper window's ID stack, so it is
      // unique per editor instance and cannot collide with host dockspaces.
      DockID dockspace_id = ImGui::GetID("##EditorDockSpace");
      ImGui::DockSpace(static_cast<ImGuiID>(dockspace_id), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
      RenderPanels(dockspace_id);
    } else {
      // Host has no docking — stack the panels inside the wrapper window.
      RenderPanelsFlat();
    }

    ImGui::End();
  } else {
    // --- host-owned layout: panels dock into the host's dockspace ----
    // No wrapper window, no toolbar — the host provides its own chrome and
    // controls the panels through SetPanelOpen / LoadSourceText & co.
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
  // vertically inside the wrapper window, each in a bordered child region.
  // The last visible panel fills the remaining height.
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
  if (!ImGui::BeginMenuBar()) return;

  // ---- file loading ----
  // Source JSON
  ImGui::SetNextItemWidth(180);
  ImGui::InputTextWithHint("##SourcePath", "source.json", source_path_, sizeof(source_path_));
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
  ImGui::SameLine();
  if (ImGui::SmallButton("...##BrowseTemplate")) {
    if (callbacks_.open_file) {
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
  // Checking a panel back on also reopens it if the user closed it via its
  // close button (the per-panel `open` flag and the toolbar toggle are
  // otherwise independent states).
  auto toggle = [](char const* label, bool& show, bool& panel_open) {
    if (ImGui::Checkbox(label, &show) && show) panel_open = true;
    ImGui::SameLine();
  };
  toggle("Input", show_input_, input_panel_.open);
  toggle("Template", show_editor_, editor_panel_.open);
  toggle("Output", show_output_, output_panel_.open);
  toggle("Monitor", show_monitor_, monitor_panel_.open);

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
