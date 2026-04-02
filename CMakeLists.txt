cmake_minimum_required(VERSION 3.21)
project(novel-downloader VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(NOVEL_ENABLE_WEBVIEW_GUI "Build the Windows WebView GUI host" ON)
set(NOVEL_WEBVIEW_VERSION "0.12.0" CACHE STRING
    "Pinned webview/webview version used by the GUI host")

if (NOT WIN32)
    message(FATAL_ERROR
        "This project only supports Windows with the MSVC toolchain.")
endif()

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR
        "This project only supports the MSVC toolchain.")
endif()

message(STATUS "Compiler family: MSVC")

include(FetchContent)

add_compile_options("$<$<COMPILE_LANGUAGE:C>:/utf-8>" "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")

# ── 依赖 ────────────────────────────────────────────────────────────────────
find_package(nlohmann_json CONFIG REQUIRED)
find_package(SQLiteCpp CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(tinyxml2 CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_path(LUABRIDGE3_INCLUDE_DIRS "luabridge3/LuaBridge/Array.h" REQUIRED)

if (NOVEL_ENABLE_WEBVIEW_GUI)
    set(WEBVIEW_BUILD_AMALGAMATION OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_ENABLE_CHECKS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_ENABLE_CLANG_FORMAT OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_ENABLE_CLANG_TIDY OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_ENABLE_PACKAGING OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_PACKAGE_AMALGAMATION OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_PACKAGE_DOCS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_PACKAGE_HEADERS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_PACKAGE_LIB OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_USE_BUILTIN_MSWEBVIEW2 ON CACHE BOOL
        "Use webview's built-in WebView2 loader implementation on Windows" FORCE)

    FetchContent_Declare(
        webview
        GIT_REPOSITORY https://github.com/webview/webview.git
        GIT_TAG        ${NOVEL_WEBVIEW_VERSION}
    )
    FetchContent_MakeAvailable(webview)
endif()

# ── 源文件 ──────────────────────────────────────────────────────────────────
set(COMMON_SOURCES
    src/source/host/http_service.cpp
    src/source/host/json_bridge.cpp
    src/source/host/host_api.cpp
    src/source/lua/lua_runtime.cpp
    src/source/lua/lua_source_schema.cpp
    src/source/lua/lua_book_source.cpp
    src/source/runtime/source_manager.cpp

    # Application Services
    src/application/library_service.cpp
    src/application/download_service.cpp
    src/application/export_service.cpp

    # 数据库层
    src/db/database.cpp

    # 导出（EPUB / TXT）
    src/export/epub_exporter.cpp
    src/export/txt_exporter.cpp
    src/export/text_sanitizer.cpp
)

add_library(novel-core STATIC
    ${COMMON_SOURCES}
)

add_library(novel::core ALIAS novel-core)

set(IS_STATIC_TRIPLET OFF)
if (DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static")
    set(IS_STATIC_TRIPLET ON)
endif()

set(NOVEL_NATIVE_TARGETS
    novel-core
)

file(GLOB_RECURSE NOVEL_PLUGIN_RESOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*"
)

if (NOVEL_ENABLE_WEBVIEW_GUI)
    file(GLOB_RECURSE NOVEL_GUI_FRONTEND_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/frontend/*"
    )
endif()

foreach(target_name IN LISTS NOVEL_NATIVE_TARGETS)
    # 静态 triplet 下优先静态链接 MSVC 运行时，便于单文件发布。
    if (IS_STATIC_TRIPLET)
        set_property(TARGET ${target_name} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LUABRIDGE3_INCLUDE_DIRS}
        ${LUA_INCLUDE_DIR}
    )

    target_link_libraries(${target_name} PRIVATE
        nlohmann_json::nlohmann_json
        SQLiteCpp
        httplib::httplib
        tinyxml2::tinyxml2
        libzip::zip
        OpenSSL::SSL
        OpenSSL::Crypto
        CLI11::CLI11
        spdlog::spdlog
        ${LUA_LIBRARIES}
    )

    target_compile_definitions(${target_name} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
        _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(${target_name} PRIVATE ws2_32)
endforeach()

if (NOVEL_ENABLE_WEBVIEW_GUI)
    add_executable(novel-downloader-gui WIN32
        src/gui/main.cpp
        src/gui/app_runtime.cpp
        src/gui/bridge.cpp
        src/gui/frontend.cpp
        src/gui/user_paths.cpp
    )

    if (IS_STATIC_TRIPLET)
        set_property(TARGET novel-downloader-gui PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    target_include_directories(novel-downloader-gui PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_definitions(novel-downloader-gui PRIVATE
        UNICODE
        _UNICODE
    )

    target_link_libraries(novel-downloader-gui PRIVATE
        novel::core
        webview::core
        nlohmann_json::nlohmann_json
        SQLiteCpp
        spdlog::spdlog
        shell32
    )

    add_custom_target(novel-downloader-gui-resources
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/src/gui/frontend
            -DDST=$<TARGET_FILE_DIR:novel-downloader-gui>/gui
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sync_resources.cmake
        COMMAND ${CMAKE_COMMAND}
            -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/plugins
            -DDST=$<TARGET_FILE_DIR:novel-downloader-gui>/plugins
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sync_resources.cmake
        DEPENDS
            ${NOVEL_GUI_FRONTEND_FILES}
            ${NOVEL_PLUGIN_RESOURCE_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sync_resources.cmake
        COMMENT "Syncing GUI frontend and plugin resources"
        VERBATIM
    )
    add_dependencies(novel-downloader-gui novel-downloader-gui-resources)

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
        add_custom_command(TARGET novel-downloader-gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/.env
                $<TARGET_FILE_DIR:novel-downloader-gui>/.env
        )
    endif()

    install(TARGETS novel-downloader-gui
        RUNTIME DESTINATION bin
    )

    install(DIRECTORY src/gui/frontend/
        DESTINATION bin/gui
    )
endif()

# ── 安装 ────────────────────────────────────────────────────────────────────
install(TARGETS novel-core
    ARCHIVE DESTINATION lib
)

install(DIRECTORY plugins/
    DESTINATION bin/plugins
)

if (CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(novel-sync-compile-commands
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
        COMMENT "Syncing compile_commands.json to repository root"
        VERBATIM
    )
endif()
