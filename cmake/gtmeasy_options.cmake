option(GTMEASY_BUILD_SHARED "Build a shared library with the C API exported" OFF)
option(GTMEASY_BUILD_TESTS "Build unit tests" OFF)
option(GTMEASY_BUILD_EXAMPLES "Build example programs" OFF)

set(GTMEASY_TRANSPORT "auto" CACHE STRING "HTTP transport: auto, winhttp, or curl")
set_property(CACHE GTMEASY_TRANSPORT PROPERTY STRINGS auto winhttp curl)

function(gtmeasy_apply_win32_defs target)
  if(WIN32)
    target_compile_definitions(${target} PRIVATE
      NOMINMAX
      WIN32_LEAN_AND_MEAN
      _CRT_SECURE_NO_WARNINGS
    )
  endif()
endfunction()
