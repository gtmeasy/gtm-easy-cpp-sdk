cmake_minimum_required(VERSION 3.21)

project(GtmEasyGrowth VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/gtmeasy_options.cmake")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(GTMEASY_TRANSPORT STREQUAL "auto")
  if(WIN32)
    set(GTMEASY_TRANSPORT_IMPL "winhttp")
  else()
    set(GTMEASY_TRANSPORT_IMPL "curl")
  endif()
else()
  set(GTMEASY_TRANSPORT_IMPL "${GTMEASY_TRANSPORT}")
endif()

if(NOT GTMEASY_TRANSPORT_IMPL STREQUAL "winhttp" AND NOT GTMEASY_TRANSPORT_IMPL STREQUAL "curl")
  message(FATAL_ERROR "GTMEASY_TRANSPORT must be auto, winhttp, or curl (got ${GTMEASY_TRANSPORT})")
endif()

find_package(nlohmann_json 3.11 CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
  set(JSON_Install ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(Threads REQUIRED)

set(GTMEASY_SOURCES
  src/c_api.cpp
  src/client.cpp
  src/flush.cpp
  src/queue.cpp
  src/storage.cpp
  src/context.cpp
  src/click_ids.cpp
  src/util.cpp
)

if(GTMEASY_TRANSPORT_IMPL STREQUAL "curl")
  list(APPEND GTMEASY_SOURCES src/transport_curl.cpp)
else()
  list(APPEND GTMEASY_SOURCES src/transport_winhttp.cpp)
endif()

if(GTMEASY_BUILD_SHARED)
  add_library(gtmeasy_growth SHARED ${GTMEASY_SOURCES})
else()
  add_library(gtmeasy_growth STATIC ${GTMEASY_SOURCES})
endif()

add_library(GtmEasy::growth ALIAS gtmeasy_growth)

target_include_directories(gtmeasy_growth
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(gtmeasy_growth PRIVATE GTMEASY_BUILDING)
if(GTMEASY_BUILD_SHARED)
  target_compile_definitions(gtmeasy_growth PUBLIC GTMEASY_SHARED)
endif()

target_link_libraries(gtmeasy_growth
  PUBLIC nlohmann_json::nlohmann_json
  PRIVATE Threads::Threads
)

set(GTMEASY_USES_CURL OFF)
if(GTMEASY_TRANSPORT_IMPL STREQUAL "curl")
  find_package(CURL REQUIRED)
  target_link_libraries(gtmeasy_growth PRIVATE CURL::libcurl)
  set(GTMEASY_USES_CURL ON)
endif()

if(GTMEASY_TRANSPORT_IMPL STREQUAL "winhttp")
  target_link_libraries(gtmeasy_growth PRIVATE winhttp advapi32)
endif()

if(APPLE)
  target_link_libraries(gtmeasy_growth PRIVATE "-framework CoreFoundation")
endif()

if(MSVC)
  target_compile_options(gtmeasy_growth PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(gtmeasy_growth PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(gtmeasy_growth PROPERTIES
  OUTPUT_NAME gtmeasy_growth
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
  EXPORT_NAME growth
)

install(TARGETS gtmeasy_growth
  EXPORT GtmEasyGrowthTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT GtmEasyGrowthTargets
  NAMESPACE GtmEasy::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GtmEasyGrowth
)

configure_package_config_file(
  cmake/GtmEasyGrowthConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/GtmEasyGrowthConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GtmEasyGrowth
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/GtmEasyGrowthConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/GtmEasyGrowthConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/GtmEasyGrowthConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GtmEasyGrowth
)

if(GTMEASY_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(GTMEASY_BUILD_EXAMPLES)
  add_subdirectory(examples/console)
endif()
