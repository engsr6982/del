#pragma once

#include <cstdint>

namespace del_editor {

/// @brief Opaque dockspace identifier (raw ImGuiID).
///
/// Keeps imgui.h out of the public headers so hosts can include the editor
/// headers before (or without) including their own imgui.h. Hosts that embed
/// the editor pass the ID of their own dockspace via EditorConfig.
using DockID = std::uint32_t;

} // namespace del_editor
