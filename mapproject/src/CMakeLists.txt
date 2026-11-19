# 收集源文件
file(GLOB_RECURSE UTILS_SOURCES  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.h")
file(GLOB        ROOT_SOURCES    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
file(GLOB        RESOURCES       CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.qrc" "${CMAKE_CURRENT_SOURCE_DIR}/resources/style/*.qss")

# core 分层收集
file(GLOB CORE_ROOT_SOURCES        CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/core/*.h")
file(GLOB_RECURSE CORE_TERRAIN_SOURCES    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/core/terrain/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/core/terrain/*.h")
file(GLOB_RECURSE CORE_CELESTIAL_SOURCES  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/core/celestial/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/core/celestial/*.h")

# UI 分层收集
file(GLOB UI_ROOT_SOURCES        CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.h")
file(GLOB_RECURSE UI_TERRAIN_SOURCES    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/terrain/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/ui/terrain/*.h")
file(GLOB_RECURSE UI_CELESTIAL_SOURCES  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/celestial/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/ui/celestial/*.h")

set(CORE_SOURCES ${CORE_ROOT_SOURCES} ${CORE_TERRAIN_SOURCES} ${CORE_CELESTIAL_SOURCES})
set(UI_SOURCES ${UI_ROOT_SOURCES} ${UI_TERRAIN_SOURCES} ${UI_CELESTIAL_SOURCES})

# 筛选器分组（只写一次，不要重复）
source_group("core"            FILES ${CORE_ROOT_SOURCES})
source_group("core\\terrain"     FILES ${CORE_TERRAIN_SOURCES})
source_group("core\\celestial"   FILES ${CORE_CELESTIAL_SOURCES})
source_group("utils"           FILES ${UTILS_SOURCES})
source_group("root"            FILES ${ROOT_SOURCES})
source_group("resources"       FILES ${RESOURCES})
source_group("ui"              FILES ${UI_ROOT_SOURCES})
source_group("ui\\terrain"     FILES ${UI_TERRAIN_SOURCES})
source_group("ui\\celestial"   FILES ${UI_CELESTIAL_SOURCES})

add_executable(MapStudio
    ${ROOT_SOURCES}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    ${RESOURCES}
)

set_target_properties(MapStudio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY          "${CMAKE_BINARY_DIR}/MapStudio"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_BINARY_DIR}/MapStudio/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_BINARY_DIR}/MapStudio/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/MapStudio/RelWithDebInfo"
    PDB_OUTPUT_DIRECTORY              "${CMAKE_BINARY_DIR}/MapStudio"
    PDB_OUTPUT_DIRECTORY_DEBUG        "${CMAKE_BINARY_DIR}/MapStudio/Debug"
    PDB_OUTPUT_DIRECTORY_RELEASE      "${CMAKE_BINARY_DIR}/MapStudio/Release"
    PDB_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/MapStudio/RelWithDebInfo"
)

target_include_directories(MapStudio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/celestial
    ${CMAKE_CURRENT_SOURCE_DIR}/core/terrain
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/celestial
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/terrain
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(MapStudio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    Qt6::Multimedia
    Qt6::3DCore
    Qt6::3DRender
    Qt6::3DInput
    Qt6::3DExtras
)

if(MSVC)
    add_compile_options(/MP /utf-8 /Zc:__cplusplus /permissive-)
endif()

# ========== 自动部署 Qt DLL ==========
set(QT_RUNTIME_CACHE "${CMAKE_BINARY_DIR}/_qt_runtime_cache")
set(QT_DEPLOY_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/deploy_qt.bat")

add_custom_target(deploy_qt_full
  COMMAND "${QT_DEPLOY_SCRIPT}" "$<TARGET_FILE_DIR:MapStudio>" "${QT_RUNTIME_CACHE}" FULL
  COMMENT "[deploy] Full Qt deploy into cache"
  VERBATIM
)
add_dependencies(deploy_qt_full MapStudio)

add_custom_command(TARGET MapStudio POST_BUILD
  COMMAND "${QT_DEPLOY_SCRIPT}" "$<TARGET_FILE_DIR:MapStudio>" "${QT_RUNTIME_CACHE}"
  COMMENT "[deploy] Sync Qt runtime -> $<TARGET_FILE_DIR:MapStudio>"
  VERBATIM
)