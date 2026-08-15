//  CodeEditor — thin wrapper around ImGuiColorTextEdit
//  Copyright (c) 2024-2026 Johan A. Goossens. All rights reserved.
//
//  This work is licensed under the terms of the MIT license.
//  For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once
#include <functional>
#include <string>

#include "TextEditor.h"

namespace del_editor {

class CodeEditor {
public:
  CodeEditor();

  /// Render the editor inline within the current ImGui parent window.
  /// The caller is responsible for Begin / End.
  void render();

  /// Access the underlying TextEditor instance.
  inline TextEditor&       getEditor() { return editor_; }
  inline TextEditor const& getEditor() const { return editor_; }

  // --- configuration ---

  /// Toggle the bottom status bar (language selector, cursor pos, etc.).
  bool showStatusBar = true;

  /// Display name shown in the status bar (e.g. filename).  Empty = hidden.
  std::string title;

  /// Called when the user wants to save.  Return true on success.
  std::function<bool()> onSave;

private:
  void renderMenuBar();
  void renderStatusBar();

  void setAutocompleteMode(bool flag);
  void buildAutocompleteTrie();

  TextEditor       editor_;
  TextEditor::Trie trie_;
  bool             autocomplete_ = false;
  float            font_size_    = 16.0f;
};

} // namespace del_editor
