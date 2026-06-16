cmake_minimum_required(VERSION 3.16)
project(PaiZhenYuanZheng LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── SFML 3 查找 ──────────────────────────────────────────────────────────────
# SFML 3 安装在 C:/SFML-3.0.2
set(SFML_DIR "C:/SFML-3.0.2/lib/cmake/SFML")
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System Audio)

# ── 源文件 ───────────────────────────────────────────────────────────────────
add_executable(PaiZhenYuanZheng
    main.cpp
    GameLogic.cpp
    AssetManager.cpp
    AudioSystem.cpp
    CardRenderer.cpp
    RunManager.cpp
    MapManager.cpp
)

target_link_libraries(PaiZhenYuanZheng PRIVATE SFML::Graphics SFML::Window SFML::System SFML::Audio)

# ── Windows 控制台窗口隐藏（Release 构建）────────────────────────────────────
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(PaiZhenYuanZheng PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── 复制 assets/ 到构建目录旁 ────────────────────────────────────────────────
add_custom_command(TARGET PaiZhenYuanZheng POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:PaiZhenYuanZheng>/assets
    COMMENT "复制 assets/ 到输出目录"
)
