cmake_minimum_required(VERSION 3.20)

project(del VERSION 0.1.0 LANGUAGES C CXX)

# ==========================================================================
# Policies
# ==========================================================================
cmake_policy(SET CMP0135 NEW)   # FetchContent downloads use timestamps

# ==========================================================================
# Options
# ==========================================================================
option(DEL_EXTERN_JSON_LIB   "Use system-installed nlohmann_json"             OFF)
option(DEL_WITH_EDITOR       "Build the del_editor library (needs ImGui)"      ON)
option(DEL_WITHOUT_TOOLS     "Disable CLI / REPL / test / editor executables" OFF)
option(DEL_EXTERN_IMGUI_LIB  "Use host-provided ImGui / GLFW / GLEW"          OFF)

# ==========================================================================
# Global compiler settings (inherit by all targets)
# ==========================================================================
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single CRT knob for Windows — no per-target overrides needed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ==========================================================================
# Version header
# ==========================================================================
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/del/version.h"
)

# ==========================================================================
# Core library: del
# ==========================================================================
set(DEL_SOURCES
    src/del/version.h
    src/del/macro.h
    src/del/exception.h        src/del/exception.cc
    src/del/lexer.h            src/del/lexer.cc
    src/del/context.h
    src/del/symbol_table.h     src/del/symbol_table.cc
    src/del/ast.h              src/del/ast.cc
    src/del/parser.h           src/del/parser.cc
    src/del/template_engine.h  src/del/template_engine.cc
)

add_library(del STATIC ${DEL_SOURCES})
add_library(del::del ALIAS del)

target_include_directories(del PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_definitions(del PUBLIC
    NOMINMAX
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:DEL_DEBUG>
)

# ---- dependency: nlohmann_json ----
include(FetchContent)

if(NOT DEL_EXTERN_JSON_LIB)
    message(STATUS "[del] fetching nlohmann_json 3.12.0 …")
    FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
    target_link_libraries(del PUBLIC nlohmann_json::nlohmann_json)
    # Treat json as a system header to suppress template-instantiation noise
    get_target_property(_json_inc nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
    if(_json_inc)
        target_include_directories(del SYSTEM PUBLIC ${_json_inc})
    endif()
endif()

# ---- compiler warnings (MSVC / clang-cl / GCC) ----
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # clang-cl (simulates MSVC) or native Clang
    target_compile_options(del PUBLIC
        -Wall
        -Wno-c++98-compat
        -Wno-reserved-identifier
        -Wno-padded
        -Wno-documentation-unknown-command
    )
elseif(MSVC)
    target_compile_options(del PUBLIC /utf-8 /W4 /sdl)
else()
    target_compile_options(del PUBLIC -Wall -Wextra)
endif()

# ==========================================================================
# Editor library: del_editor  (backend-agnostic — only depends on imgui.h)
# ==========================================================================
if(DEL_WITH_EDITOR)

    if(NOT DEL_EXTERN_IMGUI_LIB)
        # ---- ImGui v1.92.7-docking ----
        message(STATUS "[editor] fetching ImGui v1.92.7-docking …")
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
        message(STATUS "[editor] fetching GLFW 3.4 …")
        FetchContent_Declare(glfw
            URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
        )
        set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)

        # ---- ImGui backends (must follow GLFW for its include path) ----
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

        # ---- GLEW (built manually from single source — cmake-less upstream) ----
        message(STATUS "[editor] fetching GLEW 2.2.0 …")
        FetchContent_Declare(glew
            URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz
        )
        # Use FetchContent_Populate directly; GLEW has no usable root CMakeLists.txt.
        # Suppress the CMake ≥4.1 deprecation for this one call.
        set(CMAKE_WARN_DEPRECATED OFF)
        FetchContent_GetProperties(glew)
        if(NOT glew_POPULATED)
            FetchContent_Populate(glew)
        endif()
        set(CMAKE_WARN_DEPRECATED ON)

        if(NOT TARGET glew_s)
            add_library(glew_s STATIC ${glew_SOURCE_DIR}/src/glew.c)
            target_include_directories(glew_s PUBLIC ${glew_SOURCE_DIR}/include)
            target_compile_definitions(glew_s PRIVATE
                GLEW_STATIC
                $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
            )
            if(WIN32)
                target_link_libraries(glew_s PRIVATE opengl32)
            endif()
        endif()

        # ---- ImGuiColorTextEdit (engsr6982 fork) ----
        message(STATUS "[editor] fetching ImGuiColorTextEdit …")
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
    endif()

    # ---- del_editor library ----
    message(STATUS "[editor] building del_editor …")
    add_library(del_editor STATIC
        src/del-editor/bootstrap.h                  src/del-editor/bootstrap.cc
        src/del-editor/panel/input_panel.h          src/del-editor/panel/input_panel.cc
        src/del-editor/panel/editor_panel.h         src/del-editor/panel/editor_panel.cc
        src/del-editor/panel/output_panel.h         src/del-editor/panel/output_panel.cc
        src/del-editor/panel/monitor_panel.h        src/del-editor/panel/monitor_panel.cc
        src/del-editor/components/code_editor.h     src/del-editor/components/code_editor.cc
    )
    target_include_directories(del_editor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(del_editor PUBLIC del)
    if(NOT DEL_EXTERN_IMGUI_LIB)
        target_link_libraries(del_editor PUBLIC imgui_core imgui_color_text_edit)
    endif()
endif()

# ==========================================================================
# Tools (all EXCLUDE_FROM_ALL — build by name explicitly)
# ==========================================================================
if(NOT DEL_WITHOUT_TOOLS)
    message(STATUS "[tools] registering executables …")

    # del-repl
    add_executable(del_repl EXCLUDE_FROM_ALL tests/repl_main.cc)
    target_link_libraries(del_repl PRIVATE del)

    # del-cli
    add_executable(del_cli EXCLUDE_FROM_ALL tests/cli_main.cc)
    target_link_libraries(del_cli PRIVATE del)

    # del-test
    add_executable(del_test EXCLUDE_FROM_ALL tests/test_main.cc)
    target_link_libraries(del_test PRIVATE del)

    # del_live_editor  (GUI app → WIN32 hides the console)
    if(DEL_WITH_EDITOR)
        add_executable(del_live_editor WIN32 EXCLUDE_FROM_ALL tests/editor_main.cc)
        target_link_libraries(del_live_editor PRIVATE del del_editor)
        if(NOT DEL_EXTERN_IMGUI_LIB)
            target_link_libraries(del_live_editor PRIVATE imgui_backend glfw glew_s)
            target_compile_definitions(del_live_editor PRIVATE GLEW_STATIC)
        endif()
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
endif()
