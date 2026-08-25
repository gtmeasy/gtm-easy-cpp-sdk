#include "gtmeasy/growth_c.h"

const char* gtm_error_name(gtm_error_t err) {
  switch (err) {
  case GTM_OK:
    return "ok";
  case GTM_ERROR_UNKNOWN:
    return "unknown";
  case GTM_ERROR_INVALID_ARGUMENT:
    return "invalid_argument";
  case GTM_ERROR_NOT_INITIALIZED:
    return "not_initialized";
  case GTM_ERROR_NETWORK:
    return "network_error";
  case GTM_ERROR_INVALID_CREDENTIALS:
    return "invalid_credentials";
  case GTM_ERROR_RATE_LIMITED:
    return "rate_limited";
  case GTM_ERROR_SERVER:
    return "server_error";
  case GTM_ERROR_INVALID_PAYLOAD:
    return "invalid_payload";
  case GTM_ERROR_STORAGE:
    return "storage_error";
  case GTM_ERROR_TIMEOUT:
    return "timeout";
  default:
    return "unknown";
  }
}
