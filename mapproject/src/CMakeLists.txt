# src/CMakeLists.txt
find_package(Qt6 REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

add_executable(MapStudio
    main.cpp
    resources.qrc
    mapview.h
    mapview.cpp 
    MapMainWidget.h
    MapMainWidget.cpp 
)

target_link_libraries(MapStudio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    Qt6::Multimedia
)

# Windows 下自动拷贝 Conan 导入的 DLL 到运行目录
if(WIN32)
    add_custom_command(TARGET MapStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CONAN_DLL_DIR}" "$<TARGET_FILE_DIR:MapStudio>"
    )
endif()