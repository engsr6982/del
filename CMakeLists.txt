cmake_minimum_required(VERSION 3.20)

project(del VERSION 0.1.0 LANGUAGES C CXX)

# ==========================================================================
# Options & Global Settings
# ==========================================================================
option(DEL_WITH_EDITOR "Build the del_editor library" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC Runtime Settings
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ==========================================================================
# Version Header
# ==========================================================================
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/del/version.h"
)

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

# 头文件路径控制：构建时使用源码目录，安装后使用系统的 include 目录
target_include_directories(del
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# 宏定义（根据接口暴露情况使用 PUBLIC）
target_compile_definitions(del PUBLIC
    NOMINMAX
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:DEL_DEBUG>
)

# 编译器警告控制（必须为 PRIVATE，避免污染下游使用者）
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

# 依赖关联：若子 CMake 或外部环境已提供 target 则直接 link，否则由 find_package 查找
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json REQUIRED)
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

    target_include_directories(del_editor
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(del_editor PUBLIC del)

    # 透传/链接透传进来的 imgui 目标
    if(TARGET imgui_core)
        target_link_libraries(del_editor PUBLIC
            $<BUILD_INTERFACE:imgui_core>
            $<INSTALL_INTERFACE:imgui::imgui>
        )
    elseif(TARGET imgui::imgui)
        target_link_libraries(del_editor PUBLIC imgui::imgui)
    endif()

    if(TARGET imgui_color_text_edit)
        target_link_libraries(del_editor PUBLIC
            $<BUILD_INTERFACE:imgui_color_text_edit>
            $<INSTALL_INTERFACE:imgui_color_text_edit::imgui_color_text_edit>
        )
    endif()
endif()

# ==========================================================================
# Installation & Export Config
# ==========================================================================
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # 收集需要 Install 的目标
    set(DEL_TARGETS del)
    if(DEL_WITH_EDITOR)
        list(APPEND DEL_TARGETS del_editor)
    endif()

    # 1. 安装静态库二进制文件
    install(TARGETS ${DEL_TARGETS}
        EXPORT delTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # 2. 安装头文件
    install(DIRECTORY src/del
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )

    if(DEL_WITH_EDITOR)
        install(DIRECTORY src/del-editor
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
        )
    endif()

    # 3. 生成并安装 CMake 配置 (delConfig.cmake & delTargets.cmake)
    set(DEL_CONFIG_IN "${CMAKE_CURRENT_BINARY_DIR}/delConfig.cmake.in")
    file(WRITE "${DEL_CONFIG_IN}" [=[
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(nlohmann_json)

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