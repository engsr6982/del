cmake_minimum_required(VERSION 3.20)

project(del VERSION 0.1.0 LANGUAGES C CXX)

# ==========================================================================
# Options & Global Settings
# ==========================================================================
option(DEL_WITH_EDITOR "Build the del_editor library" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ==========================================================================
# Version Header
# ==========================================================================
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/del/version.h"
)

# ==========================================================================
# Dependencies
# ==========================================================================
# ---- nlohmann_json (Header Only) ----
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json QUIET)
    
    # 兼容 xmake 或裸路径环境：如果没找到 Config 文件，手动查找头文件并建立虚构目标
    if(NOT TARGET nlohmann_json::nlohmann_json AND NOT nlohmann_json_FOUND)
        find_path(NLOHMANN_JSON_INCLUDE_DIR NAMES nlohmann/json.hpp)
        if(NLOHMANN_JSON_INCLUDE_DIR)
            add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
            set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
            )
        else()
            message(FATAL_ERROR "nlohmann_json required but not found!")
        endif()
    endif()
endif()

# ---- Editor Dependencies ----
if(DEL_WITH_EDITOR)
    # 1. imgui
    if(NOT TARGET imgui::imgui)
        find_package(imgui QUIET)
        
        if(NOT TARGET imgui::imgui AND NOT imgui_FOUND)
            find_path(IMGUI_INCLUDE_DIR NAMES imgui.h)
            find_library(IMGUI_LIBRARY NAMES imgui)
            if(IMGUI_INCLUDE_DIR AND IMGUI_LIBRARY)
                add_library(imgui::imgui UNKNOWN IMPORTED)
                set_target_properties(imgui::imgui PROPERTIES
                    IMPORTED_LOCATION "${IMGUI_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${IMGUI_INCLUDE_DIR}"
                )
            else()
                message(FATAL_ERROR "imgui required but not found!")
            endif()
        endif()
    endif()

    # 2. imgui_color_text_edit
    if(NOT TARGET imgui_color_text_edit)
        find_path(IMGUI_CTE_INCLUDE_DIR NAMES TextEditor.h)
        find_library(IMGUI_CTE_LIBRARY NAMES imgui_color_text_edit)
        if(IMGUI_CTE_INCLUDE_DIR AND IMGUI_CTE_LIBRARY)
            add_library(imgui_color_text_edit UNKNOWN IMPORTED)
            set_target_properties(imgui_color_text_edit PROPERTIES
                IMPORTED_LOCATION "${IMGUI_CTE_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${IMGUI_CTE_INCLUDE_DIR}"
            )
        else()
            message(FATAL_ERROR "imgui_color_text_edit required but not found!")
        endif()
    endif()
endif()

# ==========================================================================
# Core Library: del (Static)
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

target_include_directories(del PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(del PUBLIC
    NOMINMAX
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:DEL_DEBUG>
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(del PRIVATE
        -Wall
        -Wno-c++98-compat
        -Wno-reserved-identifier
        -Wno-padded
        -Wno-documentation-unknown-command
    )
elseif(MSVC)
    target_compile_options(del PRIVATE /utf-8 /W4 /sdl)
else()
    target_compile_options(del PRIVATE -Wall -Wextra)
endif()

target_link_libraries(del PUBLIC
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
    $<INSTALL_INTERFACE:nlohmann_json::nlohmann_json>
)

# ==========================================================================
# Editor Library: del_editor (Static, Optional)
# ==========================================================================
if(DEL_WITH_EDITOR)
    set(DEL_EDITOR_SOURCES
        src/del-editor/bootstrap.h                  src/del-editor/bootstrap.cc
        src/del-editor/panel/input_panel.h          src/del-editor/panel/input_panel.cc
        src/del-editor/panel/editor_panel.h         src/del-editor/panel/editor_panel.cc
        src/del-editor/panel/output_panel.h         src/del-editor/panel/output_panel.cc
        src/del-editor/panel/monitor_panel.h        src/del-editor/panel/monitor_panel.cc
        src/del-editor/components/code_editor.h     src/del-editor/components/code_editor.cc
    )

    add_library(del_editor STATIC ${DEL_EDITOR_SOURCES})
    add_library(del::del_editor ALIAS del_editor)

    target_include_directories(del_editor PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(del_editor PUBLIC
        del
        $<BUILD_INTERFACE:imgui::imgui>
        $<INSTALL_INTERFACE:imgui::imgui>
        $<BUILD_INTERFACE:imgui_color_text_edit>
    )
endif()

# ==========================================================================
# Installation & Export Config
# ==========================================================================
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(DEL_TARGETS del)
    if(DEL_WITH_EDITOR)
        list(APPEND DEL_TARGETS del_editor)
    endif()

    install(TARGETS ${DEL_TARGETS}
        EXPORT delTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY src/del DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
    if(DEL_WITH_EDITOR)
        install(DIRECTORY src/del-editor DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
    endif()

    set(DEL_CONFIG_IN "${CMAKE_CURRENT_BINARY_DIR}/delConfig.cmake.in")
    file(WRITE "${DEL_CONFIG_IN}" [=[
@PACKAGE_INIT@
include(CMakeFindDependencyMacro)
find_dependency(nlohmann_json)
if(@DEL_WITH_EDITOR@)
    find_dependency(imgui)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/delTargets.cmake")
check_required_components(del)
]=])

    configure_package_config_file(
        "${DEL_CONFIG_IN}"
        "${CMAKE_CURRENT_BINARY_DIR}/delConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/del"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/delConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(EXPORT delTargets
        FILE delTargets.cmake
        NAMESPACE del::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/del"
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/delConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/delConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/del"
    )
endif()