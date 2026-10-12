# src/CMakeLists.txt
find_package(Qt6 REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

# ============================================================
# 按目录自动收集源文件（新增文件无需手动修改此列表）
# ============================================================
file(GLOB_RECURSE CORE_SOURCES   "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"   "${CMAKE_CURRENT_SOURCE_DIR}/core/*.h")
file(GLOB_RECURSE UI_SOURCES     "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp"     "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.h")
file(GLOB_RECURSE UTILS_SOURCES  "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp"  "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.h")
file(GLOB        ROOT_SOURCES    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
file(GLOB        RESOURCES       "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.qrc"
                                  "${CMAKE_CURRENT_SOURCE_DIR}/resources/style/*.qss")

add_executable(MapStudio
    ${ROOT_SOURCES}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    ${RESOURCES}
)

target_include_directories(MapStudio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(MapStudio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    Qt6::Multimedia
)

source_group("Core"      FILES ${CORE_SOURCES})
source_group("UI"        FILES ${UI_SOURCES})
source_group("Utils"     FILES ${UTILS_SOURCES})
source_group("Resources" FILES ${RESOURCES})

# Windows 下自动拷贝 Conan 导入的 DLL 到运行目录
if(WIN32)
    add_custom_command(TARGET MapStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CONAN_DLL_DIR}" "$<TARGET_FILE_DIR:MapStudio>"
    )
endif()