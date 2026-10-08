set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_PATCH "2")
set(VERSION_TWEAK "3")

add_compile_definitions(
  SOFTWARE_VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}"
)

# 设置版本定义选项
set(VERSION_DEFINE
    "NONE"
    CACHE STRING "Version define: SCANTECH, NEUTRAL, BIYING, or NONE")

# 验证只能选择一个版本
set(ALLOWED_VERSIONS "NONE" "SCANTECH" "NEUTRAL" "BIYING")
if(NOT VERSION_DEFINE IN_LIST ALLOWED_VERSIONS)
  message(STATUS "VERSION_DEFINE: ${VERSION_DEFINE}")
  message(
    FATAL_ERROR "VERSION_DEFINE must be one of: NONE, SCANTECH, NEUTRAL, BIYING"
  )
endif()

# 根据选择的版本定义相应的宏
if(VERSION_DEFINE STREQUAL "SCANTECH")
  message(STATUS "Building with SCANTECH_VERSION")
  add_compile_definitions(SCANTECH_VERSION)

  message(STATUS "Using SCANTECH_VERSION Info")
  set(COMPANY_NAME "SCANTECH (HANGZHOU) CO., LTD.")
  set(FILE_DESCRIPTION "3DeVOK Studio")
  set(INTERNAL_NAME "3DeVOK")
  set(LEGAL_COPYRIGHT "Copyright (C) 2026  SCANTECH (HANGZHOU) CO., LTD.")
  set(ORIGINAL_FILENAME "3DeVOK Studio")
  set(PRODUCT_NAME "3DeVOK Studio")
  set(ICON_PATH
      "${CMAKE_CURRENT_SOURCE_DIR}/src/app_3devok/resources/icon/3DeVOK.ico")
  set(EXECUTABLE_NAME "3DeVOK Studio")
elseif(VERSION_DEFINE STREQUAL "NEUTRAL")
  message(STATUS "Building with NEUTRAL_VERSION")
  add_compile_definitions(NEUTRAL_VERSION)

  message(STATUS "Using NEUTRAL_VERSION Info")
  set(COMPANY_NAME "")
  set(FILE_DESCRIPTION "3D Color Scanner")
  set(INTERNAL_NAME "3D Color Scanner")
  set(LEGAL_COPYRIGHT "Copyright 2026.All Rights Reserved")
  set(ORIGINAL_FILENAME "3D Color Scanner")
  set(PRODUCT_NAME "3D Color Scanner")
  set(ICON_PATH
      "${CMAKE_CURRENT_SOURCE_DIR}/src/app_3devok/resources/icon/Neutral.ico")
  set(EXECUTABLE_NAME "3D Color Scanner")
elseif(VERSION_DEFINE STREQUAL "BIYING")
  message(STATUS "Building with BIYING_VERSION")
  add_compile_definitions(BIYING_VERSION)

  message(STATUS "Using BIYING_VERSION Info")
  set(COMPANY_NAME "")
  set(FILE_DESCRIPTION "3D Color Scanner")
  set(INTERNAL_NAME "3D Color Scanner")
  set(LEGAL_COPYRIGHT "Copyright 2026.All Rights Reserved")
  set(ORIGINAL_FILENAME "3D Color Scanner")
  set(PRODUCT_NAME "3D Color Scanner")
  set(ICON_PATH
      "${CMAKE_CURRENT_SOURCE_DIR}/src/app_3devok/resources/icon/Biying.ico")
  set(EXECUTABLE_NAME "3D Color Scanner")
else()
  message(STATUS "Not define VERSION, Building with SCANTECH_VERSION")
  add_compile_definitions(SCANTECH_VERSION)

  message(STATUS "Not define VERSION, Using SCANTECH_VERSION Info")
  set(COMPANY_NAME "SCANTECH (HANGZHOU) CO., LTD.")
  set(FILE_DESCRIPTION "3DeVOK Studio")
  set(INTERNAL_NAME "3DeVOK")
  set(LEGAL_COPYRIGHT "Copyright (C) 2026  SCANTECH (HANGZHOU) CO., LTD.")
  set(ORIGINAL_FILENAME "3DeVOK Studio")
  set(PRODUCT_NAME "3DeVOK Studio")
  set(ICON_PATH
      "${CMAKE_CURRENT_SOURCE_DIR}/src/app_3devok/resources/icon/3DeVOK.ico")
  set(EXECUTABLE_NAME "3DeVOK Studio")
endif()

add_compile_definitions(COMPANY_IDENTIFY="ScanTech")
add_compile_definitions(SOFTWARE_IDENTIFY="Scanner")
add_compile_definitions(SOFTWARE_NAME="${PRODUCT_NAME}")
add_compile_definitions(SOFTWARE_INTERNAL_NAME="${INTERNAL_NAME}")
