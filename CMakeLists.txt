cmake_minimum_required(VERSION 3.21)
project(novel-downloader VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NOVEL_COMPILER_FAMILY "${CMAKE_CXX_COMPILER_ID}")
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(NOVEL_COMPILER_FAMILY "MSVC")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(NOVEL_COMPILER_FAMILY "Clang")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(NOVEL_COMPILER_FAMILY "GNU")
endif()

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR
        "The native GUI bridge currently requires the MSVC toolchain.")
endif()

message(STATUS "Compiler family: ${NOVEL_COMPILER_FAMILY}")

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

set(GUI_BACKEND_SOURCES
    src/gui/backend/backend_bridge.cpp
    src/gui/backend/bridge_exports.cpp
)

set(RESOURCE_FILES)
if (WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources.rc")
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/logo.ico")
        list(APPEND RESOURCE_FILES resources.rc)
    else()
        message(WARNING "logo.ico not found; skipping resources.rc for this build.")
    endif()
endif()

add_library(novel-gui-bridge SHARED
    ${COMMON_SOURCES}
    ${GUI_BACKEND_SOURCES}
    ${RESOURCE_FILES}
)

set(IS_STATIC_TRIPLET OFF)
if (DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static")
    set(IS_STATIC_TRIPLET ON)
endif()

set(NOVEL_NATIVE_TARGETS
    novel-gui-bridge
)

foreach(target_name IN LISTS NOVEL_NATIVE_TARGETS)
    # 静态 triplet 下优先静态链接 MSVC 运行时，便于单文件发布。
    if (MSVC AND IS_STATIC_TRIPLET)
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

    # Windows 下防止控制台乱码
    if (WIN32)
        target_compile_definitions(${target_name} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _WIN32_WINNT=0x0A00
            _CRT_SECURE_NO_WARNINGS
        )
        target_link_libraries(${target_name} PRIVATE ws2_32)

        # 动态链接场景下，构建后自动把第三方 DLL 复制到目标目录，
        # 便于直接打包整个 release 目录发布。
        if (NOT IS_STATIC_TRIPLET)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${target_name}>
                    $<TARGET_FILE_DIR:${target_name}>
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
                    $<TARGET_FILE_DIR:${target_name}>/plugins
                COMMAND_EXPAND_LISTS
            )
        else()
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/plugins
                    $<TARGET_FILE_DIR:${target_name}>/plugins
            )
        endif()
    endif()
endforeach()

# ── 安装 ────────────────────────────────────────────────────────────────────
install(TARGETS novel-gui-bridge
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)

install(DIRECTORY plugins/
    DESTINATION bin/plugins
)

if (WIN32 AND NOT IS_STATIC_TRIPLET)
    install(FILES $<TARGET_RUNTIME_DLLS:novel-gui-bridge>
        DESTINATION bin
    )
endif()
