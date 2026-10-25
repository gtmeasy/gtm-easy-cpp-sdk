# GTM Easy C++ SDK

First-party C++ SDK for [GTM Easy](https://gtmeasy.com) growth analytics, attribution, surveys, and paywall-funnel events. Sends events to the GTM Easy ingestion API, identifies users, persists an anonymous ID across launches, and captures system context (OS version, device model, locale, timezone).

**Windows desktop first** (Win32 / Qt / Electron native modules / game engines), designed to be portable to macOS and Linux hosts.

Sibling SDKs: [Apple (Swift)](https://github.com/gtmeasy/gtm-easy-apple-sdk) · [Android (Kotlin)](https://github.com/gtmeasy/gtm-easy-android-sdk) · [Web (TypeScript)](https://github.com/gtmeasy/gtm-easy-js-sdk) · [Flutter (Dart)](https://github.com/gtmeasy/gtm-easy-flutter-sdk)

## Status

`0.2.0` — adds `suppressTestBuilds` / `buildChannel` (see [Suppressing test builds](#suppressing-test-builds)). Public surface is the C ABI in `include/gtmeasy/growth_c.h` and the C++17 wrapper in `include/gtmeasy/growth.h`.

## Layout

```
include/gtmeasy/   public headers (C API, C++ wrapper, event name constants)
src/               SDK implementation (queue, persistence, HTTP transport)
examples/console   identify + track + flush sample
tests/             doctest suite (in-process mock server)
ports/             vcpkg overlay port
```

## Usage

```cpp
#include <gtmeasy/events.h>
#include <gtmeasy/growth.h>

gtmeasy::Config cfg;
cfg.apiKey = "gtm_growth_...";
cfg.app = "my-app";
cfg.cacheDir = gtmeasy::defaultCacheDir("MyCompany", "MyApp");

auto analytics = gtmeasy::GrowthAnalytics::create(cfg);
if (!analytics) {
  return;
}
analytics->identify("user_123", {{"plan", "pro"}});
analytics->track(gtmeasy::events::paywall_opened, {{"placement", "onboarding"}});
analytics->flush();
```

`track()` / `identify()` never block on the network: they validate, enqueue, and return. Call `flush()` (or destroy the client) to wait for delivery.

Version string: `gtmeasy::GrowthAnalytics::version()` (currently `"0.2.0"`).

## Suppressing test builds

Keep debug / TestFlight / internal test builds from sending events (install
counts, funnels and notifications stay clean). The SDK does **not** detect the
channel itself — it is compiled separately from your app, so its `NDEBUG` says
nothing about your build — pass it in:

```cpp
gtmeasy::Config cfg;
// ...
cfg.suppressTestBuilds = true;
#ifndef NDEBUG
cfg.buildChannel = gtmeasy::BuildChannel::Debug;
#else
cfg.buildChannel = gtmeasy::BuildChannel::Production;
#endif
// cfg.disabled = true;   // hard off switch, regardless of channel

auto analytics = gtmeasy::GrowthAnalytics::create(cfg);
if (analytics && analytics->isSuppressed()) { /* events are dropped */ }
```

Rule: `suppressed = disabled || (suppressTestBuilds && buildChannel ∈ {Debug, TestFlight, InternalTest})`.
`BuildChannel::Unknown` (the default) is never suppressed. When suppressed, every
call still returns `GTM_OK` but nothing is sent and nothing is written to
`cacheDir`: no worker thread, no flush (explicit or on destruction), the on-disk
queue is left untouched, `trackFirstOpen()` records no version baseline (so the
later production build still sends `app.first_open`), and `identify()` / `reset()`
/ click ids only change memory. An existing anonymous id is reused; otherwise an
in-memory one is generated. One info-level log line is emitted at create.

C API — the original `gtm_growth_config_t` layout is unchanged; use the versioned extension:

```c
gtm_growth_config_ext_t ext;
gtm_growth_config_ext_init(&ext);          /* sets ext.struct_size */
ext.suppress_test_builds = 1;
ext.build_channel = GTM_BUILD_CHANNEL_DEBUG;
gtm_growth_t* client = NULL;
gtm_growth_create_ext(&cfg, &ext, &client); /* ext may be NULL */
int suppressed = gtm_growth_is_suppressed(client);
```

## Integration

The SDK ships as a CMake package. Target: `GtmEasy::growth` · package name: `GtmEasyGrowth` · vcpkg port: `gtmeasy-growth`.

### Requirements

- CMake ≥ 3.21, a C++17 compiler (MSVC 2019 16.11+, Clang 12+, GCC 9+)
- Windows: no third-party network dependency (WinHTTP + Schannel). macOS/Linux: libcurl (resolved by vcpkg/Conan or the system).
- JSON: nlohmann/json (header-only, fetched automatically when not found).

### Option A — vcpkg (recommended on Windows)

`vcpkg.json` in your project:

```json
{
  "dependencies": [
    { "name": "gtmeasy-growth", "version>=": "0.2.0" }
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
  GIT_TAG        v0.2.0
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
  std::printf("GTM Easy growth SDK %s\n", gtmeasy::GrowthAnalytics::version());
}
```

Build with `cmake --build build --config Release` and run — if it prints the version, the include path, library, and CRT match.

## License

MIT © GTM Easy — <hi@gtmeasy.com>
