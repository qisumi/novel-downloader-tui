cmake_minimum_required(VERSION 3.21)
project(novel-downloader-tui VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── 依赖 ────────────────────────────────────────────────────────────────────
find_package(ftxui CONFIG REQUIRED)
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
set(SOURCES
    src/main.cpp

    # Source / Plugin Runtime
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

    # TUI 界面
    src/tui/app.cpp
    src/tui/screens/search_screen.cpp
    src/tui/screens/bookshelf_screen.cpp
    src/tui/screens/book_detail_screen.cpp
    src/tui/components/loading_indicator.cpp
)

set(RESOURCE_FILES resources.rc)

add_executable(${PROJECT_NAME} ${SOURCES})

set(IS_STATIC_TRIPLET OFF)
if (DEFINED VCPKG_TARGET_TRIPLET AND VCPKG_TARGET_TRIPLET MATCHES "-static")
    set(IS_STATIC_TRIPLET ON)
endif()

# 静态 triplet 下优先静态链接 MSVC 运行时，便于单文件发布。
if (MSVC AND IS_STATIC_TRIPLET)
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LUABRIDGE3_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
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
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
        _CRT_SECURE_NO_WARNINGS
    )
    # 链接 ws2_32（httplib 需要）
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)

    # 动态链接场景下，构建后自动把第三方 DLL 复制到 exe 同目录，
    # 便于直接打包整个 release 目录发布。
    if (NOT IS_STATIC_TRIPLET)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/plugins
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins
            COMMAND_EXPAND_LISTS
        )
    else()
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/plugins
                $<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins
        )
    endif()
endif()

# ── 安装 ────────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(DIRECTORY plugins/
    DESTINATION bin/plugins
)

if (WIN32 AND NOT IS_STATIC_TRIPLET)
    install(FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
        DESTINATION bin
    )
endif()
