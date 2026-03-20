#include "app_mode.h"

#include <string.h>
#include "app_config.h"
#include "measurement_report.h"
#include "nvm3.h"
#include "nvm3_default.h"

#define APP_MODE_OVERRIDE_NVM_KEY     0x4001u
#define APP_MODE_OVERRIDE_MAGIC       0xA5u
#define APP_MODE_OVERRIDE_VERSION     0x01u

typedef struct {
  uint8_t magic;
  uint8_t version;
  uint8_t mode;
  uint8_t reserved;
} app_mode_override_storage_t;

static app_mode_state_t app_mode_state = {
  .active_mode = APP_MODE_CS_INITIATOR,
  .default_mode = APP_MODE_CS_INITIATOR,
  .override_valid = false,
  .override_mode = APP_MODE_CS_INITIATOR,
  .source = APP_MODE_SOURCE_CONFIG,
};

static app_mode_t mode_from_config(void);
static bool is_valid_mode(uint8_t mode);
static sl_status_t load_override(app_mode_override_storage_t *storage);
static sl_status_t save_override(app_mode_t mode);

void app_mode_init(void)
{
  app_mode_state.default_mode = mode_from_config();
  app_mode_state.active_mode = app_mode_state.default_mode;
  app_mode_state.override_valid = false;
  app_mode_state.override_mode = app_mode_state.default_mode;
  app_mode_state.source = APP_MODE_SOURCE_CONFIG;
}

sl_status_t app_mode_handle_system_boot(void)
{
  app_mode_override_storage_t storage;
  sl_status_t sc = load_override(&storage);

  app_mode_state.active_mode = app_mode_state.default_mode;
  app_mode_state.override_valid = false;
  app_mode_state.override_mode = app_mode_state.default_mode;
  app_mode_state.source = APP_MODE_SOURCE_CONFIG;

  if (sc == SL_STATUS_OK) {
    app_mode_state.override_valid = true;
    app_mode_state.override_mode = (app_mode_t)storage.mode;
    app_mode_state.active_mode = app_mode_state.override_mode;
    app_mode_state.source = APP_MODE_SOURCE_SERIAL;
    return SL_STATUS_OK;
  }

  if (sc == SL_STATUS_NOT_FOUND) {
    return SL_STATUS_OK;
  }

  if (sc == SL_STATUS_INVALID_STATE) {
    return SL_STATUS_OK;
  }

  if (sc == SL_STATUS_FAIL) {
    return SL_STATUS_OK;
  }

  measurement_report_emit_app_error("app_mode",
                                    "override_load_failed",
                                    sc);
  return SL_STATUS_OK;
}

const app_mode_state_t *app_mode_get_state(void)
{
  return &app_mode_state;
}

bool app_mode_is_beacon_scan(void)
{
  return (app_mode_state.active_mode == APP_MODE_BEACON_SCAN);
}

sl_status_t app_mode_request_switch(app_mode_t mode, app_mode_source_t source)
{
  sl_status_t sc;

  if (!is_valid_mode((uint8_t)mode)) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  sc = save_override(mode);
  if (sc != SL_STATUS_OK) {
    measurement_report_emit_app_error("app_mode",
                                      "override_save_failed",
                                      sc);
    return sc;
  }

  measurement_report_emit_app_mode_switch("switch_requested",
                                          app_mode_to_str(app_mode_state.active_mode),
                                          app_mode_to_str(mode),
                                          app_mode_source_to_str(source),
                                          true);
  sl_bt_system_reboot();
  return SL_STATUS_OK;
}

sl_status_t app_mode_clear_override(app_mode_source_t source)
{
  sl_status_t sc = nvm3_deleteObject(nvm3_defaultHandle, APP_MODE_OVERRIDE_NVM_KEY);

  if (sc != SL_STATUS_OK && sc != SL_STATUS_NOT_FOUND) {
    measurement_report_emit_app_error("app_mode",
                                      "override_clear_failed",
                                      sc);
    return sc;
  }

  measurement_report_emit_app_mode_switch("override_cleared",
                                          app_mode_to_str(app_mode_state.active_mode),
                                          app_mode_to_str(app_mode_state.default_mode),
                                          app_mode_source_to_str(source),
                                          true);
  sl_bt_system_reboot();
  return SL_STATUS_OK;
}

void app_mode_process_action(void)
{
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
    case APP_MODE_SOURCE_SERIAL:
      return "serial";
    case APP_MODE_SOURCE_BUTTON:
      return "button";
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

static bool is_valid_mode(uint8_t mode)
{
  return (mode == APP_MODE_CS_INITIATOR || mode == APP_MODE_BEACON_SCAN);
}

static sl_status_t load_override(app_mode_override_storage_t *storage)
{
  size_t value_len = 0u;
  sl_status_t sc;

  if (storage == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memset(storage, 0, sizeof(*storage));
  sc = nvm3_readData(nvm3_defaultHandle,
                     APP_MODE_OVERRIDE_NVM_KEY,
                     storage,
                     sizeof(*storage));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  value_len = sizeof(*storage);

  if (value_len != sizeof(*storage)
      || storage->magic != APP_MODE_OVERRIDE_MAGIC
      || storage->version != APP_MODE_OVERRIDE_VERSION
      || !is_valid_mode(storage->mode)) {
    (void)nvm3_deleteObject(nvm3_defaultHandle, APP_MODE_OVERRIDE_NVM_KEY);
    return SL_STATUS_INVALID_STATE;
  }

  return SL_STATUS_OK;
}

static sl_status_t save_override(app_mode_t mode)
{
  app_mode_override_storage_t storage = {
    .magic = APP_MODE_OVERRIDE_MAGIC,
    .version = APP_MODE_OVERRIDE_VERSION,
    .mode = (uint8_t)mode,
    .reserved = 0u,
  };

  return nvm3_writeData(nvm3_defaultHandle,
                        APP_MODE_OVERRIDE_NVM_KEY,
                        &storage,
                        sizeof(storage));
}
