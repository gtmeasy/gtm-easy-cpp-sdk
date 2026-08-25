# GTM Easy C++ SDK

First-party C++ SDK for [GTM Easy](https://gtmeasy.com) growth analytics, attribution, surveys, and paywall-funnel events. Sends events to the GTM Easy ingestion API, identifies users, persists an anonymous ID across launches, and captures system context (OS version, device model, locale, timezone).

**Windows desktop first** (Win32 / Qt / Electron native modules / game engines), designed to be portable to macOS and Linux hosts.

Sibling SDKs: [Apple (Swift)](https://github.com/gtmeasy/gtm-easy-apple-sdk) · [Android (Kotlin)](https://github.com/gtmeasy/gtm-easy-android-sdk) · [Web (TypeScript)](https://github.com/gtmeasy/gtm-easy-js-sdk) · [Flutter (Dart)](https://github.com/gtmeasy/gtm-easy-flutter-sdk)

## Status

`0.1.0` — scaffold. API surface, build system, and the generated OpenAPI client are being designed; nothing is published yet.

## Planned layout

```
include/gtmeasy/   public headers (C++17, stable C API shell planned)
src/               SDK implementation (event queue, persistence, HTTP transport)
generated/         OpenAPI-generated API client (do not edit by hand)
examples/          minimal Win32 / console samples
tests/
```

## Planned usage

```cpp
#include <gtmeasy/growth.h>

auto analytics = gtmeasy::GrowthAnalytics::create({ .apiKey = "gtm_growth_..." });
analytics->identify("user_123", {{"plan", "pro"}});
analytics->track("paywall.shown", {{"placement", "onboarding"}});
analytics->flush();
```

## Integration

The SDK ships as a CMake package. Target: `GtmEasy::growth` · package name: `GtmEasyGrowth` · vcpkg port: `gtmeasy-growth`.

> Status: the CMake layout and target names below are the contract for 0.1.0; the vcpkg port lands with the first buildable release. Until then use FetchContent against a tag.

### Requirements

- CMake ≥ 3.21, a C++17 compiler (MSVC 2019 16.11+, Clang 12+, GCC 9+)
- Windows: no third-party network dependency (WinHTTP + Schannel). macOS/Linux: libcurl (resolved by vcpkg/Conan or the system).
- JSON: nlohmann/json (header-only, fetched automatically when not found).

### Option A — vcpkg (recommended on Windows)

`vcpkg.json` in your project:

```json
{
  "dependencies": [
    { "name": "gtmeasy-growth", "version>=": "0.1.0" }
  ]
}
```

Until the port is in the vcpkg registry, point vcpkg at the overlay shipped in this repo:

```powershell
cmake -B build -S . `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_OVERLAY_PORTS="<path-to>/gtm-easy-cpp-sdk/ports" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
```

`x64-windows-static` produces a fully static library (no DLLs to redistribute). Use `x64-windows` if your app links the dynamic CRT; do not mix — the SDK must use the same runtime (`/MT` vs `/MD`) as your executable.

Then in `CMakeLists.txt`:

```cmake
find_package(GtmEasyGrowth CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE GtmEasy::growth)
```

### Option B — CMake FetchContent (no package manager)

```cmake
include(FetchContent)
FetchContent_Declare(
  gtmeasy_growth
  GIT_REPOSITORY https://github.com/gtmeasy/gtm-easy-cpp-sdk.git
  GIT_TAG        v0.1.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(gtmeasy_growth)

target_link_libraries(my_app PRIVATE GtmEasy::growth)
```

Build options you can set before `FetchContent_MakeAvailable`:

| Option | Default | Meaning |
|---|---|---|
| `GTMEASY_BUILD_SHARED` | `OFF` | Build a DLL / `.so` with the C API exported instead of a static lib |
| `GTMEASY_BUILD_TESTS` | `OFF` | Build unit tests (uses a local cpp-httplib mock server) |
| `GTMEASY_BUILD_EXAMPLES` | `OFF` | Build `examples/` |
| `GTMEASY_TRANSPORT` | `auto` | `winhttp` on Windows, `curl` elsewhere; force one to override |

### Option C — prebuilt binaries

Each tagged release will attach `gtmeasy-growth-<version>-windows-x64-static.zip` (and `-x64` / `-arm64` DLL variants) containing `include/`, `lib/`, and `lib/cmake/GtmEasyGrowth/`. Unzip and point CMake at it:

```cmake
list(APPEND CMAKE_PREFIX_PATH "C:/deps/gtmeasy-growth")
find_package(GtmEasyGrowth CONFIG REQUIRED)
```

### Using the C API from other toolchains

`include/gtmeasy/growth_c.h` is the stable ABI boundary (`extern "C"`, opaque handles, POD structs, JSON passed as UTF-8 strings). Link the static lib or DLL from any language with a C FFI (Rust, C#, Python ctypes, Delphi) without the C++ wrapper.

### Verifying the link

```cpp
#include <gtmeasy/growth.h>
#include <cstdio>

int main() {
  std::printf("GTM Easy growth SDK %s\n", gtmeasy::version());
}
```

Build with `cmake --build build --config Release` and run — if it prints the version, the include path, library, and CRT match.

## License

MIT © GTM Easy — <hi@gtmeasy.com>
