cmake_minimum_required(VERSION 3.23)
project(MapProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ================= 引入本地QT =================
set(LOCAL_QT_DIR "C:/Qt/6.12.0/msvc2022_64" CACHE PATH "Local Qt installation directory")
if(NOT EXISTS "${LOCAL_QT_DIR}")
  message(FATAL_ERROR "Qt 路径不存在，请检查: ${LOCAL_QT_DIR}")
endif()
list(APPEND CMAKE_PREFIX_PATH "${LOCAL_QT_DIR}")

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "禁止源码目录构建 (In-source builds)")
endif()

# ================= 查找并配置 QT (全局设置) =================
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Network Multimedia)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

# ================= 编译选项与输出目录 =================
if(MSVC)
  add_compile_options(/MP /utf-8 /Zc:__cplusplus /permissive-)
else()
  add_compile_options(-finput-charset=utf-8 -fexec-charset=GBK)
endif()

set(APP_RUNNING_DIR "${CMAKE_BINARY_DIR}/MapStudio")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${APP_RUNNING_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${APP_RUNNING_DIR})
file(MAKE_DIRECTORY ${APP_RUNNING_DIR})

# ================= 包含子项目 =================
# 在顶层配置好一切后，包含子目录
add_subdirectory(src)