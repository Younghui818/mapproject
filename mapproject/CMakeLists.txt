cmake_minimum_required(VERSION 3.23)
project(MapProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ================= 1. 基础工程规范 =================
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds not allowed. Please create a build directory.\n")
endif()

# 平台检测 (保留基础宏定义)
if(WIN32)
  set(PLATFORM_WINDOWS 1)
elseif(APPLE)
  set(PLATFORM_MACOS 1)
  set(CMAKE_BUILD_RPATH "@loader_path")
  set(CMAKE_INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set(PLATFORM_LINUX 1)
endif()

# MSVC 编译优化
if(MSVC)
  add_compile_options(/MP /utf-8 /Zc:__cplusplus /permissive-)
  add_compile_options("$<$<CONFIG:Release>:/Zi>")
  add_link_options("$<$<CONFIG:Release>:/DEBUG>")
  add_link_options("$<$<CONFIG:Release>:/INCREMENTAL>")
endif()

# ================= 2. 路径与品牌配置 =================
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

# 如果 branding.cmake 不存在则使用默认值
if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/branding.cmake)
  include(${CMAKE_SOURCE_DIR}/cmake/branding.cmake)
else()
  set(PRODUCT_NAME "MapStudio")
endif()

set(APP_RUNNING_NAME "${PRODUCT_NAME}")
set(APP_RUNNING_DIR "${CMAKE_BINARY_DIR}/${APP_RUNNING_NAME}")
set(CONAN_DLL_DIR "${APP_RUNNING_DIR}/conan_dlls")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${APP_RUNNING_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${APP_RUNNING_DIR})
file(MAKE_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# ================= 3. Conan 依赖配置 (极简版) =================
include(${CMAKE_SOURCE_DIR}/cmake/conan.cmake)

  list(APPEND CONAN_IMPORTS
    "bin, *.db -> ${CONAN_DLL_DIR}"
    "Configuration, *.* -> ${CONAN_DLL_DIR}/Configuration"
    "bin, crashpad_handler.exe -> ${CONAN_DLL_DIR}"
    "bin, *.dll -> ${CONAN_DLL_DIR}"
    "bin, *.pdb -> ${CONAN_DLL_DIR}"
    "bin/pdb, *.pdb -> ${CONAN_DLL_DIR}"
    "pdb, *.* -> ${CONAN_DLL_DIR}"
    "plugins, *.* -> ${CONAN_DLL_DIR}/plugins"
    "resources, *.* -> ${CONAN_DLL_DIR}/resources"
    "bin/rtsp_server, *.* -> ${CONAN_DLL_DIR}/rtsp_server"
    "bin/MeasCadConverter, * -> ${CONAN_DLL_DIR}/MeasCadConverter"
  )

# 仅保留 Demo 必需的核心依赖
conan_cmake_configure(
  REQUIRES
    qt/6.8.3
  OPTIONS
    qt:shared=True
    qt:qtsvg=True
    qt:qttools=True
    qt:qtmultimedia=True
    qt:qtscxml=True
    GENERATORS
    CMakeDeps
    CMakeToolchain
    IMPORTS
    ${CONAN_IMPORTS}
)

set(CONAN_INSTALL_SETTINGS qt/*:compiler.cppstd=17)
if(MSVC)
  list(APPEND CONAN_INSTALL_SETTINGS compiler.version=17)
endif()

conan_cmake_install(
  PATH_OR_REFERENCE .
  REMOTE conan-local
  BUILD never
  SETTINGS ${CONAN_INSTALL_SETTINGS}
  OPTIONS tools.system.package_manager:mode=disable
)

# ================= 4. 子项目 =================
add_subdirectory(src)