//  CodeEditor — thin wrapper around ImGuiColorTextEdit
//  Copyright (c) 2024-2026 Johan A. Goossens. All rights reserved.
//
//  This work is licensed under the terms of the MIT license.
//  For a copy, see <https://opensource.org/licenses/MIT>.

#include "code_editor.h"

#include "imgui.h"

#if __APPLE__
#define SHORTCUT "Cmd-"
#else
#define SHORTCUT "Ctrl-"
#endif

namespace del_editor {

// --- construction ----------------------------------------------------------

CodeEditor::CodeEditor() { editor_.SetLanguage(TextEditor::Language::Cpp()); }

// --- render ----------------------------------------------------------------

void CodeEditor::render() {
  // menu bar (no-op if parent window lacks ImGuiWindowFlags_MenuBar)
  renderMenuBar();

  // editor widget — reserve space for status bar if visible
  auto  area     = ImGui::GetContentRegionAvail();
  auto& style    = ImGui::GetStyle();
  float status_h = showStatusBar ? ImGui::GetFrameHeight() + 2.0f * style.WindowPadding.y + style.ItemSpacing.y : 0.0f;
  auto  editorSize = ImVec2(0.0f, area.y - status_h);

  ImGui::PushFont(nullptr, font_size_);
  editor_.Render("TextEditor", editorSize);
  ImGui::PopFont();

  if (showStatusBar) {
    ImGui::Spacing();
    renderStatusBar();
  }
}

// --- menu bar --------------------------------------------------------------

void CodeEditor::renderMenuBar() {
  if (!ImGui::BeginMenuBar()) return;

  // ---- Edit ----
  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem("Undo", " " SHORTCUT "Z", nullptr, editor_.CanUndo())) editor_.Undo();
#if __APPLE__
    if (ImGui::MenuItem("Redo", "^" SHORTCUT "Z", nullptr, editor_.CanRedo())) editor_.Redo();
#else
    if (ImGui::MenuItem("Redo", " " SHORTCUT "Y", nullptr, editor_.CanRedo())) editor_.Redo();
#endif

    ImGui::Separator();
    if (ImGui::MenuItem("Cut", " " SHORTCUT "X", nullptr, editor_.AnyCursorHasSelection())) editor_.Cut();
    if (ImGui::MenuItem("Copy", " " SHORTCUT "C", nullptr, editor_.AnyCursorHasSelection())) editor_.Copy();
    if (ImGui::MenuItem("Paste", " " SHORTCUT "V", nullptr, ImGui::GetClipboardText() != nullptr)) editor_.Paste();

    ImGui::Separator();
    bool flag = editor_.IsInsertSpacesOnTabs();
    if (ImGui::MenuItem("Insert Spaces on Tabs", nullptr, &flag)) editor_.SetInsertSpacesOnTabs(flag);

    if (ImGui::MenuItem("Tabs To Spaces")) editor_.TabsToSpaces();
    if (ImGui::MenuItem("Spaces To Tabs", nullptr, nullptr, !editor_.IsInsertSpacesOnTabs())) editor_.SpacesToTabs();
    if (ImGui::MenuItem("Strip Trailing Whitespaces")) editor_.StripTrailingWhitespaces();

    ImGui::EndMenu();
  }

  // ---- Selection ----
  if (ImGui::BeginMenu("Selection")) {
    if (ImGui::MenuItem("Select All", " " SHORTCUT "A", nullptr, !editor_.IsEmpty())) editor_.SelectAll();
    ImGui::Separator();

    if (ImGui::MenuItem("Indent Line(s)", " " SHORTCUT "]", nullptr, !editor_.IsEmpty())) editor_.IndentLines();
    if (ImGui::MenuItem("Deindent Line(s)", " " SHORTCUT "[", nullptr, !editor_.IsEmpty())) editor_.DeindentLines();
    if (ImGui::MenuItem("Move Line(s) Up", nullptr, nullptr, !editor_.IsEmpty())) editor_.MoveUpLines();
    if (ImGui::MenuItem("Move Line(s) Down", nullptr, nullptr, !editor_.IsEmpty())) editor_.MoveDownLines();
    if (ImGui::MenuItem("Toggle Comments", " " SHORTCUT "/", nullptr, editor_.HasLanguage())) editor_.ToggleComments();
    ImGui::Separator();

    if (ImGui::MenuItem("To Uppercase", nullptr, nullptr, editor_.AnyCursorHasSelection()))
      editor_.SelectionToUpperCase();
    if (ImGui::MenuItem("To Lowercase", nullptr, nullptr, editor_.AnyCursorHasSelection()))
      editor_.SelectionToLowerCase();
    ImGui::Separator();

    if (ImGui::MenuItem("Add Next Occurrence", " " SHORTCUT "D", nullptr, editor_.CurrentCursorHasSelection()))
      editor_.AddNextOccurrence();
    if (ImGui::MenuItem("Select All Occurrences", "^" SHORTCUT "D", nullptr, editor_.CurrentCursorHasSelection()))
      editor_.SelectAllOccurrences();

    ImGui::EndMenu();
  }

  // ---- View ----
  if (ImGui::BeginMenu("View")) {
    if (ImGui::MenuItem("Zoom In", " " SHORTCUT "+")) font_size_ = std::clamp(font_size_ + 1.0f, 8.0f, 24.0f);
    if (ImGui::MenuItem("Zoom Out", " " SHORTCUT "-")) font_size_ = std::clamp(font_size_ - 1.0f, 8.0f, 24.0f);
    ImGui::Separator();

    bool flag;
    if (ImGui::MenuItem("Autocomplete", nullptr, &autocomplete_)) setAutocompleteMode(autocomplete_);

    ImGui::Separator();

    ImGui::MenuItem("Show Status Bar", nullptr, &showStatusBar);

    flag = editor_.IsShowWhitespacesEnabled();
    if (ImGui::MenuItem("Show Whitespaces", nullptr, &flag)) editor_.SetShowWhitespacesEnabled(flag);

    flag = editor_.IsShowSpacesEnabled();
    if (ImGui::MenuItem("Show Spaces", nullptr, &flag)) editor_.SetShowSpacesEnabled(flag);

    flag = editor_.IsShowTabsEnabled();
    if (ImGui::MenuItem("Show Tabs", nullptr, &flag)) editor_.SetShowTabsEnabled(flag);

    flag = editor_.IsShowLineNumbersEnabled();
    if (ImGui::MenuItem("Show Line Numbers", nullptr, &flag)) editor_.SetShowLineNumbersEnabled(flag);

    flag = editor_.IsShowingMatchingBrackets();
    if (ImGui::MenuItem("Show Matching Brackets", nullptr, &flag)) editor_.SetShowMatchingBrackets(flag);

    flag = editor_.IsCompletingPairedGlyphs();
    if (ImGui::MenuItem("Complete Matching Glyphs", nullptr, &flag)) editor_.SetCompletePairedGlyphs(flag);

    flag = editor_.IsShowPanScrollIndicatorEnabled();
    if (ImGui::MenuItem("Show Pan/Scroll Indicator", nullptr, &flag)) editor_.SetShowPanScrollIndicatorEnabled(flag);

    flag = editor_.IsMiddleMousePanMode();
    if (ImGui::MenuItem("Middle Mouse Pan Mode", nullptr, &flag)) {
      if (flag) editor_.SetMiddleMousePanMode();
      else editor_.SetMiddleMouseScrollMode();
    }

    ImGui::EndMenu();
  }

  // ---- Find ----
  if (ImGui::BeginMenu("Find")) {
    if (ImGui::MenuItem("Find", " " SHORTCUT "F")) editor_.OpenFindReplaceWindow();
    if (ImGui::MenuItem("Find Next", " " SHORTCUT "G", nullptr, editor_.HasFindString())) editor_.FindNext();
    if (ImGui::MenuItem("Find All", "^" SHORTCUT "G", nullptr, editor_.HasFindString())) editor_.FindAll();

    ImGui::EndMenu();
  }

  ImGui::EndMenuBar();

  // keyboard shortcuts
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantCaptureKeyboard) {
    if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
      if (ImGui::IsKeyPressed(ImGuiKey_Equal)) font_size_ = std::clamp(font_size_ + 1.0f, 8.0f, 24.0f);
      else if (ImGui::IsKeyPressed(ImGuiKey_Minus)) font_size_ = std::clamp(font_size_ - 1.0f, 8.0f, 24.0f);
    }
  }
}

// --- status bar ------------------------------------------------------------

void CodeEditor::renderStatusBar() {
  static const char* languages[] =
      {"C", "C++", "Cs", "AngelScript", "Lua", "Python", "GLSL", "HLSL", "JSON", "Markdown", "SQL"};
  static const TextEditor::Language* definitions[] = {
      TextEditor::Language::C(),
      TextEditor::Language::Cpp(),
      TextEditor::Language::Cs(),
      TextEditor::Language::AngelScript(),
      TextEditor::Language::Lua(),
      TextEditor::Language::Python(),
      TextEditor::Language::Glsl(),
      TextEditor::Language::Hlsl(),
      TextEditor::Language::Json(),
      TextEditor::Language::Markdown(),
      TextEditor::Language::Sql()
  };

  ImGui::BeginChild("StatusBar", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

  // language selector
  std::string lang = editor_.GetLanguageName();
  ImGui::SetNextItemWidth(120.0f);
  if (ImGui::BeginCombo("##LanguageSelector", lang.c_str())) {
    for (int n = 0; n < IM_ARRAYSIZE(languages); n++) {
      bool selected = (lang == languages[n]);
      if (ImGui::Selectable(languages[n], selected)) {
        editor_.SetLanguage(definitions[n]);
        buildAutocompleteTrie();
      }
      if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  // cursor position & title, right-aligned
  int   line, column;
  float glyphWidth = ImGui::CalcTextSize("#").x;
  editor_.GetCurrentCursor(line, column);

  auto width = ImGui::GetContentRegionAvail().x;

  // Draw save indicator (red dot = onSave set, i.e. "needs saving")
  ImGui::SameLine(0.0f, 0.0f);
  ImGui::AlignTextToFramePadding();
  {
    auto  drawlist = ImGui::GetWindowDrawList();
    auto  pos      = ImGui::GetCursorScreenPos();
    float offset   = ImGui::GetFrameHeight() * 0.5f;
    float radius   = offset * 0.6f;
    ImU32 color    = onSave ? IM_COL32(164, 0, 0, 255) : IM_COL32(164, 164, 164, 255);
    drawlist->AddCircleFilled(ImVec2(pos.x + offset, pos.y + offset), radius, color);
  }

  // Cursor info + title
  char rhs[256];
  int  rhsLen;
  if (!title.empty()) {
    rhsLen = std::snprintf(
        rhs,
        sizeof(rhs),
        "Ln %d, Col %d  Tab Size: %d  File: %s",
        line + 1,
        column + 1,
        editor_.GetTabSize(),
        title.c_str()
    );
  } else {
    rhsLen = std::snprintf(rhs, sizeof(rhs), "Ln %d, Col %d  Tab Size: %d", line + 1, column + 1, editor_.GetTabSize());
  }
  float rhsWidth = glyphWidth * (rhsLen + 2);
  ImGui::SameLine(0.0f, std::max(0.0f, width - rhsWidth - 18.0f));
  ImGui::TextUnformatted(rhs);

  ImGui::EndChild();
}

// --- autocomplete ----------------------------------------------------------

void CodeEditor::setAutocompleteMode(bool flag) {
  if (flag) {
    buildAutocompleteTrie();

    TextEditor::AutoCompleteConfig config;
    config.callback = [this](TextEditor::AutoCompleteState& state) {
      trie_.findSuggestions(state.suggestions, state.searchTerm);
    };
    editor_.SetAutoCompleteConfig(&config);
    editor_.SetChangeCallback([this]() { buildAutocompleteTrie(); }, 3000);
  } else {
    editor_.SetAutoCompleteConfig(nullptr);
    editor_.SetChangeCallback(nullptr);
  }
}

void CodeEditor::buildAutocompleteTrie() {
  trie_.clear();
  auto* lang = editor_.GetLanguage();
  if (lang) {
    for (auto& w : lang->keywords) trie_.insert(w);
    for (auto& w : lang->declarations) trie_.insert(w);
    for (auto& w : lang->identifiers) trie_.insert(w);
  }
  editor_.IterateIdentifiers([this](std::string const& id) { trie_.insert(id); });
}

} // namespace del_editor
