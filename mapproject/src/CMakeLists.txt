# 收集源文件（注意路径现在要加上 src/）
file(GLOB_RECURSE CORE_SOURCES   "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/core/*.h")
file(GLOB_RECURSE UI_SOURCES     "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.h")
file(GLOB_RECURSE UTILS_SOURCES  "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.h")
file(GLOB        ROOT_SOURCES    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
file(GLOB        RESOURCES       "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.qrc" "${CMAKE_CURRENT_SOURCE_DIR}/resources/style/*.qss")

add_executable(MapStudio
    ${ROOT_SOURCES}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${UTILS_SOURCES}
    ${RESOURCES}
)

# 包含目录 (使用 CMAKE_SOURCE_DIR 确保跨层级路径正确)
target_include_directories(MapStudio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# 链接库
target_link_libraries(MapStudio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    Qt6::Multimedia
)

# 源码分组显示
source_group("Core"      FILES ${CORE_SOURCES})
source_group("UI"        FILES ${UI_SOURCES})
source_group("Utils"     FILES ${UTILS_SOURCES})
source_group("Resources" FILES ${RESOURCES})

# ========== 编译选项 ==========
if(MSVC)
    add_compile_options(/MP /utf-8 /Zc:__cplusplus /permissive-)
endif()

# ========== 自动部署 Qt DLL (替代 copy_directory) ==========
if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core) # 确保找到 windeployqt
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    
    add_custom_command(TARGET MapStudio POST_BUILD
        COMMAND "${_qt_bin_dir}/windeployqt"
                --no-translations 
                --no-opengl-sw
                --no-compiler-runtime
                "$<TARGET_FILE_DIR:MapStudio>/$<TARGET_FILE_NAME:MapStudio>"
        COMMENT "Deploying Qt DLLs to executable directory..."
    )
endif()