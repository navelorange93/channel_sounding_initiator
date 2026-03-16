#include "app_mode.h"

#include "app_config.h"

static app_mode_state_t app_mode_state = {
  .active_mode = APP_MODE_CS_INITIATOR,
  .default_mode = APP_MODE_CS_INITIATOR,
  .override_valid = false,
  .override_mode = APP_MODE_CS_INITIATOR,
  .source = APP_MODE_SOURCE_CONFIG,
};

static app_mode_t mode_from_config(void);

void app_mode_init(void)
{
  app_mode_state.default_mode = mode_from_config();
  app_mode_state.active_mode = app_mode_state.default_mode;
  app_mode_state.override_valid = false;
  app_mode_state.override_mode = app_mode_state.default_mode;
  app_mode_state.source = APP_MODE_SOURCE_CONFIG;
}

const app_mode_state_t *app_mode_get_state(void)
{
  return &app_mode_state;
}

bool app_mode_is_beacon_scan(void)
{
  return (app_mode_state.active_mode == APP_MODE_BEACON_SCAN);
}

const char *app_mode_to_str(app_mode_t mode)
{
  switch (mode) {
    case APP_MODE_BEACON_SCAN:
      return "ble";
    case APP_MODE_CS_INITIATOR:
    default:
      return "cs";
  }
}

const char *app_mode_source_to_str(app_mode_source_t source)
{
  switch (source) {
    case APP_MODE_SOURCE_CONFIG:
    default:
      return "config";
  }
}

static app_mode_t mode_from_config(void)
{
  switch (APP_RADIO_MODE) {
    case APP_RADIO_MODE_BEACON_SCAN:
      return APP_MODE_BEACON_SCAN;
    case APP_RADIO_MODE_CS_INITIATOR:
    default:
      return APP_MODE_CS_INITIATOR;
  }
}
