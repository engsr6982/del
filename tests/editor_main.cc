// DEL Live Editor — standalone executable
//
// This file initialises the ImGui + GLFW + OpenGL3 backend, creates an
// EditorBootstrap instance, and runs the main event loop.
//
// The editor library (del_editor) itself has zero knowledge of GLFW or
// OpenGL — it only calls standard ImGui APIs.  Other host applications
// can embed the same EditorBootstrap into their own ImGui programs.

#include "del-editor/bootstrap.h"

// Windows headers must come before glfw3 to avoid APIENTRY redefinition
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// --- error helpers ----------------------------------------------------------

static void GlfwErrorCallback(int /*error*/, char const* description) {
#ifdef _WIN32
  MessageBoxA(nullptr, description, "GLFW Error", MB_OK | MB_ICONERROR);
#else
  std::fprintf(stderr, "[GLFW] %s\n", description);
#endif
}

[[noreturn]] static void Die(char const* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char buf[1024];
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
#ifdef _WIN32
  MessageBoxA(nullptr, buf, "Fatal Error", MB_OK | MB_ICONERROR);
#else
  std::fprintf(stderr, "%s\n", buf);
#endif
  std::exit(EXIT_FAILURE);
}

// --- font loading -----------------------------------------------------------

/// Load Consolas (monospace) as the primary font, then merge a CJK font
/// (Microsoft YaHei / PingFang / Noto Sans CJK) for Chinese glyph coverage.
/// ImGui ≥ 1.92 auto-rasterises glyphs on demand, so we do NOT specify
/// explicit glyph ranges or call atlas->Build() — both are deprecated.
static void LoadFonts(float base_size = 18.0f) {
  ImGuiIO&     io    = ImGui::GetIO();
  ImFontAtlas* atlas = io.Fonts;

  // ---- base font: Consolas (monospace, ships with Windows) ----
#ifdef _WIN32
  char const* consolas_path = "C:\\Windows\\Fonts\\consola.ttf";
#elif __APPLE__
  char const* consolas_path = "/System/Library/Fonts/Menlo.ttc";
#else
  char const* consolas_path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
#endif

  ImFont* base = atlas->AddFontFromFileTTF(consolas_path, base_size, nullptr,
                                            atlas->GetGlyphRangesDefault());
  if (!base) {
    base = atlas->AddFontDefault(); // fallback: built-in font
  }

  // ---- CJK font (merged) ----
  ImFontConfig merge_cfg;
  merge_cfg.MergeMode     = true;
  merge_cfg.OversampleH   = 2;
  merge_cfg.OversampleV   = 2;

  // Merge mode: glyphs not found in the base font are pulled from here.
  // ImGui ≥ 1.92 auto-rasterises needed glyphs → no ranges needed.
#ifdef _WIN32
  char const* cjk_path = "C:\\Windows\\Fonts\\msyh.ttc";
#elif __APPLE__
  char const* cjk_path = "/System/Library/Fonts/PingFang.ttc";
#else
  char const* cjk_path = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc";
#endif

  atlas->AddFontFromFileTTF(cjk_path, base_size, &merge_cfg,
                             atlas->GetGlyphRangesDefault());
  // CJK load failure is non-fatal — ASCII text still renders fine
}

// --- native file dialog (Windows) ------------------------------------------

#ifdef _WIN32
static std::string NativeOpenFileDialog(char const* filter_hint) {
  // Build a simple filter string from the hint
  char filter[256] = {};
  if (filter_hint && std::strcmp(filter_hint, "json") == 0) {
    std::snprintf(filter, sizeof(filter),
                  "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0");
  } else {
    std::snprintf(filter, sizeof(filter), "All Files (*.*)\0*.*\0");
  }

  char filename[MAX_PATH] = {};

  OPENFILENAMEA ofn       = {};
  ofn.lStructSize         = sizeof(ofn);
  ofn.hwndOwner           = nullptr;
  ofn.lpstrFilter         = filter;
  ofn.lpstrFile           = filename;
  ofn.nMaxFile            = sizeof(filename);
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
              OFN_NOCHANGEDIR;

  if (GetOpenFileNameA(&ofn)) {
    return std::string(filename);
  }
  return {};
}
#else
static std::string NativeOpenFileDialog(char const* /*filter_hint*/) {
  return {}; // not implemented on this platform
}
#endif

// --- main ------------------------------------------------------------------

int main(int, char**) {
  // -- glfw --
  glfwSetErrorCallback(GlfwErrorCallback);
  if (!glfwInit()) Die("Failed to initialise GLFW");

  // OpenGL 3.3 core
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow* window =
      glfwCreateWindow(1280, 800, "DEL Live Editor", nullptr, nullptr);
  if (!window) Die("Failed to create GLFW window");

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // vsync

  // -- glew (must be after context creation) --
  glewExperimental = GL_TRUE;
  GLenum glew_err  = glewInit();
  if (glew_err != GLEW_OK) {
    Die("Failed to initialise GLEW: %s",
        reinterpret_cast<char const*>(glewGetErrorString(glew_err)));
  }
  glGetError(); // consume pending error from GLEW init

  // -- imgui context --
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Persist layout to a file inside the project so the user can delete it
  io.IniFilename = nullptr; // we'll handle this explicitly if needed later

  // style
  ImGui::StyleColorsDark();

  // -- load fonts (Consolas + Chinese subset) --
  LoadFonts(18.0f);

  // -- imgui backends --
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");

  // -- editor --
  del_editor::EditorBootstrap editor;

  // Wire host callbacks (native file dialog)
  del_editor::EditorHostCallbacks cbs;
  cbs.open_file = [](char const* filter) -> std::string {
    return NativeOpenFileDialog(filter);
  };
  editor.SetHostCallbacks(std::move(cbs));

  // -- main loop --
  while (!glfwWindowShouldClose(window) && editor.IsOpen()) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render editor (fills the entire viewport — no ImGui::DockSpaceOverViewport)
    editor.Render();

    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // -- shutdown --
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
