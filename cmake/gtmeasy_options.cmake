option(GTMEASY_BUILD_SHARED "Build a shared library with the C API exported" OFF)
option(GTMEASY_BUILD_TESTS "Build unit tests" OFF)
option(GTMEASY_BUILD_EXAMPLES "Build example programs" OFF)

set(GTMEASY_TRANSPORT "auto" CACHE STRING "HTTP transport: auto, winhttp, or curl")
set_property(CACHE GTMEASY_TRANSPORT PROPERTY STRINGS auto winhttp curl)
