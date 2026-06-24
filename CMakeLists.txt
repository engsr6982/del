cmake_minimum_required(VERSION 3.20)

project(del VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DEL_SOURCES
        src/del/macro.h
        src/del/parser.h
        src/del/parser.cc
)

add_library(del STATIC ${DEL_SOURCES})
add_library(del::del ALIAS del)

target_include_directories(del PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(del PUBLIC
        NOMINMAX
        UNICODE
        _UNICODE
        $<$<CONFIG:Debug>:DEL_DEBUG>
)

if (MSVC)
    target_compile_options(del PUBLIC /utf-8 /W4 /sdl)
    set_property(TARGET del PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
else()
    target_compile_options(del PUBLIC -Wall -Wextra)
endif()


add_executable(del_test EXCLUDE_FROM_ALL tests/main.cc)
target_link_libraries(del_test PRIVATE del)