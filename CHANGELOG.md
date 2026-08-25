# Changelog

## 0.1.0 — 2026-08-25

First release of the GTM Easy C++ growth SDK.

- C ABI (`gtmeasy/growth_c.h`) plus a C++17 wrapper (`gtmeasy/growth.h`)
- Disk-backed JSONL queue, anonymous/device identity, and a background flush worker
- libcurl transport on macOS/Linux and WinHTTP on Windows
- Batch `/events/batch` with fallback to `/events` when the server returns 404
- Click-id capture (`gclid`, `fbclid`, …) with 90-day TTL and Facebook `fbc`/`fbp` derivation
- Device context (`properties._ctx`) and `track_first_open` lifecycle events
- doctest suite against an in-process mock server; console example; CMake + vcpkg overlay
