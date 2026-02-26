cmake_minimum_required(VERSION 3.20)
project(fanqie-downloader-tui VERSION 0.1.0 LANGUAGES CXX)

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

# ── 源文件 ──────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp

    # 数据层 / API
    src/api/fanqie_client.cpp

    # 数据库层
    src/db/database.cpp

    # 导出（EPUB / TXT）
    src/export/epub_exporter.cpp
    src/export/txt_exporter.cpp
    src/export/text_sanitizer.cpp

    # TUI 界面
    src/ui/app.cpp
    src/ui/screens/search_screen.cpp
    src/ui/screens/bookshelf_screen.cpp
    src/ui/screens/book_detail_screen.cpp
    src/ui/components/loading_indicator.cpp
)

set(RESOURCE_FILES resources.rc)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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
endif()

# ── 安装 ────────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
