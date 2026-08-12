cmake_minimum_required(VERSION 3.20)

project(del_workspace LANGUAGES C CXX)

# ==========================================================================
# Global Settings
# ==========================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_policy(SET CMP0135 NEW)

# ==========================================================================
# 1. Fetch External Dependencies
# ==========================================================================
include(FetchContent)

# ---- nlohmann_json ----
message(STATUS "[deps] fetching nlohmann_json 3.12.0 ...")
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

# ---- ImGui ----
message(STATUS "[deps] fetching ImGui v1.92.7-docking ...")
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/b1bcb12a624af7509894c8e77dd47416997777fa.tar.gz
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_core STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui_core SYSTEM PUBLIC ${imgui_SOURCE_DIR})
target_compile_definitions(imgui_core PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# ---- GLFW ----
message(STATUS "[deps] fetching GLFW 3.4 ...")
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# ---- ImGui backends ----
add_library(imgui_backend STATIC
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_backend PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)
target_link_libraries(imgui_backend PUBLIC imgui_core)

# ---- GLEW (自定义定义 target，避开 GLEW 内置老旧 CMake) ----
message(STATUS "[deps] fetching GLEW 2.2.0 ...")
FetchContent_Declare(glew
    URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz
)
FetchContent_MakeAvailable(glew)

if(NOT TARGET glew_s)
    add_library(glew_s STATIC ${glew_SOURCE_DIR}/src/glew.c)
    target_include_directories(glew_s PUBLIC ${glew_SOURCE_DIR}/include)
    target_compile_definitions(glew_s PUBLIC GLEW_STATIC)
    if(WIN32)
        target_link_libraries(glew_s PRIVATE opengl32)
    endif()
endif()

# ---- ImGuiColorTextEdit ----
message(STATUS "[deps] fetching ImGuiColorTextEdit ...")
FetchContent_Declare(ImGuiColorTextEdit
    URL https://github.com/engsr6982/ImGuiColorTextEdit/archive/refs/heads/master.tar.gz
)
FetchContent_MakeAvailable(ImGuiColorTextEdit)

add_library(imgui_color_text_edit STATIC
    ${imguicolortextedit_SOURCE_DIR}/TextEditor.cpp
    ${imguicolortextedit_SOURCE_DIR}/TextEditor.h
    ${imguicolortextedit_SOURCE_DIR}/dtl.h
    ${imguicolortextedit_SOURCE_DIR}/TextDiff.h
    ${imguicolortextedit_SOURCE_DIR}/TextDiff.cpp
)
target_include_directories(imgui_color_text_edit SYSTEM PUBLIC ${imguicolortextedit_SOURCE_DIR})
target_link_libraries(imgui_color_text_edit PUBLIC imgui_core)

# ==========================================================================
# 2. Reverse Include Main CMake (Parent Directory)
# ==========================================================================
set(DEL_WITH_EDITOR ON CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}/del_core")

# ==========================================================================
# 3. Tool Executables
# ==========================================================================
message(STATUS "[tools] registering executables ...")

add_executable(del_repl EXCLUDE_FROM_ALL repl_main.cc)
target_link_libraries(del_repl PRIVATE del)

add_executable(del_cli EXCLUDE_FROM_ALL cli_main.cc)
target_link_libraries(del_cli PRIVATE del)

add_executable(del_test EXCLUDE_FROM_ALL test_main.cc)
target_link_libraries(del_test PRIVATE del)

if(DEL_WITH_EDITOR)
    add_executable(del_live_editor WIN32 EXCLUDE_FROM_ALL editor_main.cc)
    target_link_libraries(del_live_editor PRIVATE del del_editor imgui_backend glfw glew_s)
    target_compile_definitions(del_live_editor PRIVATE GLEW_STATIC)

    if(WIN32)
        target_link_libraries(del_live_editor PRIVATE opengl32)
        target_link_options(del_live_editor PRIVATE "/ENTRY:mainCRTStartup")
    elseif(APPLE)
        find_library(COCOA_LIBRARY Cocoa REQUIRED)
        find_library(OPENGL_LIBRARY OpenGL REQUIRED)
        find_library(IOKIT_LIBRARY IOKit REQUIRED)
        target_link_libraries(del_live_editor PRIVATE
            ${COCOA_LIBRARY} ${OPENGL_LIBRARY} ${IOKIT_LIBRARY})
    else()
        target_link_libraries(del_live_editor PRIVATE GL X11)
    endif()
endif()