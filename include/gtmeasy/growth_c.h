#ifndef GTMEASY_GROWTH_C_H
#define GTMEASY_GROWTH_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(GTMEASY_SHARED)
#ifdef GTMEASY_BUILDING
#define GTMEASY_API __declspec(dllexport)
#else
#define GTMEASY_API __declspec(dllimport)
#endif
#else
#define GTMEASY_API
#endif

typedef struct gtm_growth gtm_growth_t;

typedef enum gtm_error {
  GTM_OK = 0,
  GTM_ERROR_UNKNOWN = 1,
  GTM_ERROR_INVALID_ARGUMENT = 2,
  GTM_ERROR_NOT_INITIALIZED = 3,
  GTM_ERROR_NETWORK = 10,
  GTM_ERROR_INVALID_CREDENTIALS = 11,
  GTM_ERROR_RATE_LIMITED = 12,
  GTM_ERROR_SERVER = 13,
  GTM_ERROR_INVALID_PAYLOAD = 20,
  GTM_ERROR_STORAGE = 30,
  GTM_ERROR_TIMEOUT = 40
} gtm_error_t;

typedef enum gtm_log_level {
  GTM_LOG_DEBUG = 0,
  GTM_LOG_INFO = 1,
  GTM_LOG_WARN = 2,
  GTM_LOG_ERROR = 3,
  GTM_LOG_NONE = 4
} gtm_log_level_t;

typedef void (*gtm_log_fn)(gtm_log_level_t level, const char* message, void* user_data);

typedef struct gtm_growth_config {
  const char* api_key;     /* required */
  const char* app;         /* required: app slug */
  const char* environment; /* "production" (default) | "staging" | "development" */
  const char* base_url;    /* default "https://www.gtmeasy.com/api/v1/growth" */
  const char* cache_dir;   /* required: directory the SDK owns (created if missing) */
  const char* app_version; /* optional */
  const char* build_number; /* optional */
  int flush_at;            /* default 20 events */
  int flush_interval_ms;   /* default 30000 */
  int max_queue_size;      /* default 1000 events on disk; oldest dropped beyond */
  int request_timeout_ms;  /* default 10000 */
  int use_batch_endpoint;  /* default 1 */
  gtm_log_level_t log_level; /* default GTM_LOG_WARN */
  gtm_log_fn log_fn;       /* optional; NULL = stderr */
  void* log_user_data;
} gtm_growth_config_t;

/* Build channel of the host app. The SDK never auto-detects it (the library is
 * compiled separately from the host, so its NDEBUG says nothing about the app);
 * pass it explicitly, e.g. `#ifndef NDEBUG` -> GTM_BUILD_CHANNEL_DEBUG. */
typedef enum gtm_build_channel {
  GTM_BUILD_CHANNEL_UNKNOWN = 0, /* never suppressed */
  GTM_BUILD_CHANNEL_DEBUG = 1,
  GTM_BUILD_CHANNEL_TESTFLIGHT = 2,
  GTM_BUILD_CHANNEL_INTERNAL_TEST = 3,
  GTM_BUILD_CHANNEL_PRODUCTION = 4
} gtm_build_channel_t;

/* Versioned extension to gtm_growth_config_t (whose layout is frozen).
 * Always call gtm_growth_config_ext_init() first: it sets struct_size so the
 * library only reads fields the caller's header knew about.
 *
 * Suppressed = disabled || (suppress_test_builds && build_channel is DEBUG,
 * TESTFLIGHT or INTERNAL_TEST). A suppressed client sends nothing, starts no
 * worker thread and writes nothing to cache_dir. */
typedef struct gtm_growth_config_ext {
  uint32_t struct_size;     /* sizeof(gtm_growth_config_ext_t); set by _init */
  int suppress_test_builds; /* default 0 */
  int disabled;             /* default 0 */
  int build_channel;        /* gtm_build_channel_t; default GTM_BUILD_CHANNEL_UNKNOWN */
} gtm_growth_config_ext_t;

GTMEASY_API void gtm_growth_config_init(gtm_growth_config_t* config);
GTMEASY_API gtm_error_t gtm_growth_create(const gtm_growth_config_t* config, gtm_growth_t** out);
GTMEASY_API void gtm_growth_config_ext_init(gtm_growth_config_ext_t* ext);
/* ext may be NULL (same as gtm_growth_create). ext->struct_size == 0 is
 * GTM_ERROR_INVALID_ARGUMENT; fields beyond struct_size use their defaults. */
GTMEASY_API gtm_error_t gtm_growth_create_ext(const gtm_growth_config_t* config,
                                              const gtm_growth_config_ext_t* ext,
                                              gtm_growth_t** out);
GTMEASY_API void gtm_growth_destroy(gtm_growth_t* client);

GTMEASY_API gtm_error_t gtm_growth_identify(gtm_growth_t* client,
                                            const char* user_id,
                                            const char* username,
                                            const char* email,
                                            const char* traits_json);
GTMEASY_API gtm_error_t gtm_growth_track(gtm_growth_t* client,
                                         const char* event_name,
                                         const char* properties_json);
GTMEASY_API gtm_error_t gtm_growth_track_metric(gtm_growth_t* client,
                                                const char* event_name,
                                                const char* properties_json,
                                                double metric_value,
                                                const char* metric_label);
GTMEASY_API gtm_error_t gtm_growth_track_first_open(gtm_growth_t* client);
GTMEASY_API gtm_error_t gtm_growth_record_click_id(gtm_growth_t* client,
                                                   const char* provider,
                                                   const char* value);
GTMEASY_API int gtm_growth_capture_click_ids(gtm_growth_t* client, const char* url);
GTMEASY_API gtm_error_t gtm_growth_flush(gtm_growth_t* client, int timeout_ms);
GTMEASY_API gtm_error_t gtm_growth_reset(gtm_growth_t* client);

/* 1 when the client drops all events (see gtm_growth_config_ext_t), else 0. */
GTMEASY_API int gtm_growth_is_suppressed(gtm_growth_t* client);
GTMEASY_API gtm_build_channel_t gtm_growth_build_channel(gtm_growth_t* client);

GTMEASY_API const char* gtm_growth_anonymous_id(gtm_growth_t* client);
GTMEASY_API const char* gtm_growth_user_id(gtm_growth_t* client);
GTMEASY_API const char* gtm_growth_version(void);
GTMEASY_API const char* gtm_error_name(gtm_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* GTMEASY_GROWTH_C_H */
