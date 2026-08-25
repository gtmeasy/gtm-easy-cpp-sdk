include(FetchContent)

# doctest 2.4.11 still declares cmake_minimum_required(< 3.5)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(doctest)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.3
  GIT_SHALLOW TRUE
)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)

add_executable(gtmeasy_growth_tests
  main.cpp
  test_util.cpp
  test_storage.cpp
  test_click_ids.cpp
  test_context.cpp
  test_client.cpp
  test_c_api.cpp
)

target_include_directories(gtmeasy_growth_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(gtmeasy_growth_tests PRIVATE
  gtmeasy_growth
  doctest::doctest
  httplib::httplib
  Threads::Threads
)

if(MSVC)
  target_compile_options(gtmeasy_growth_tests PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(gtmeasy_growth_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME gtmeasy_growth_tests COMMAND gtmeasy_growth_tests)
