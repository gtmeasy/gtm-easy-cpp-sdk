# Changelog

## 0.2.0 — 2026-10-25

- `Config::suppressTestBuilds`, `Config::disabled` and `Config::buildChannel`
  (`gtmeasy::BuildChannel`: Debug, TestFlight, InternalTest, Production, Unknown).
  Suppressed = `disabled || (suppressTestBuilds && channel is Debug/TestFlight/InternalTest)`;
  `Unknown` (default) is never suppressed. No auto-detection — the host passes the channel.
- Suppressed clients make no network calls, start no worker thread and write nothing to
  `cacheDir` (queue, `last_version`, ids, click ids untouched), so a later production build
  on the same machine still sends `app.first_open`.
- C API: versioned `gtm_growth_config_ext_t` (`struct_size`-checked) with
  `gtm_growth_config_ext_init`, `gtm_growth_create_ext`, `gtm_growth_is_suppressed` and
  `gtm_growth_build_channel`. `gtm_growth_config_t` layout is unchanged.
- `GrowthAnalytics::isSuppressed()` / `buildChannel()`.

## 0.1.0 — 2026-08-25

First release of the GTM Easy C++ growth SDK.

- C ABI (`gtmeasy/growth_c.h`) plus a C++17 wrapper (`gtmeasy/growth.h`)
- Disk-backed JSONL queue, anonymous/device identity, and a background flush worker
- libcurl transport on macOS/Linux and WinHTTP on Windows
- Batch `/events/batch` with fallback to `/events` when the server returns 404
- Click-id capture (`gclid`, `fbclid`, …) with 90-day TTL and Facebook `fbc`/`fbp` derivation
- Device context (`properties._ctx`) and `track_first_open` lifecycle events
- doctest suite against an in-process mock server; console example; CMake + vcpkg overlay
