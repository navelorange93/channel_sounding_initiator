#ifndef APP_MODE_H
#define APP_MODE_H

#include <stdbool.h>
#include "sl_bt_api.h"
#include "sl_status.h"

typedef enum {
  APP_MODE_CS_INITIATOR = 0,
  APP_MODE_BEACON_SCAN,
} app_mode_t;

typedef enum {
  APP_MODE_SOURCE_CONFIG = 0,
  APP_MODE_SOURCE_SERIAL,
  APP_MODE_SOURCE_BUTTON,
} app_mode_source_t;

typedef struct {
  app_mode_t active_mode;
  app_mode_t default_mode;
  bool override_valid;
  app_mode_t override_mode;
  app_mode_source_t source;
} app_mode_state_t;

/**************************************************************************//**
 * Initialize runtime mode state from static configuration.
 *****************************************************************************/
void app_mode_init(void);

/**************************************************************************//**
 * Resolve the active mode once the Bluetooth stack is ready.
 *****************************************************************************/
sl_status_t app_mode_handle_system_boot(void);

/**************************************************************************//**
 * Return the current mode selection state.
 *****************************************************************************/
const app_mode_state_t *app_mode_get_state(void);

/**************************************************************************//**
 * Return true when beacon scan mode is active.
 *****************************************************************************/
bool app_mode_is_beacon_scan(void);

/**************************************************************************//**
 * Request switching to the given mode and reboot.
 *****************************************************************************/
sl_status_t app_mode_request_switch(app_mode_t mode, app_mode_source_t source);

/**************************************************************************//**
 * Clear any persisted override and reboot into the config default.
 *****************************************************************************/
sl_status_t app_mode_clear_override(app_mode_source_t source);

/**************************************************************************//**
 * Reboot when a mode change is pending.
 *****************************************************************************/
void app_mode_process_action(void);

/**************************************************************************//**
 * Convert a mode enum to its wire-format string.
 *****************************************************************************/
const char *app_mode_to_str(app_mode_t mode);

/**************************************************************************//**
 * Convert a source enum to its wire-format string.
 *****************************************************************************/
const char *app_mode_source_to_str(app_mode_source_t source);

#endif // APP_MODE_H
