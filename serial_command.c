#include "serial_command.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "app_mode.h"
#include "measurement_report.h"
#include "sl_iostream.h"
#include "sl_iostream_handles.h"
#include "sl_iostream_init_eusart_instances.h"
#include "sl_status.h"

#define SERIAL_COMMAND_BUFFER_SIZE 64u

static char command_buffer[SERIAL_COMMAND_BUFFER_SIZE];
static size_t command_length = 0u;

static void process_input_byte(char byte);
static void process_command_line(const char *command);
static void emit_mode_query(void);
static bool is_blank_line(const char *command);

void serial_command_init(void)
{
  memset(command_buffer, 0, sizeof(command_buffer));
  command_length = 0u;
}

void serial_command_process_action(void)
{
  char rx_buffer[16];
  size_t bytes_read = 0u;
  sl_status_t sc = sl_iostream_read(sl_iostream_vcom_handle,
                                    rx_buffer,
                                    sizeof(rx_buffer),
                                    &bytes_read);

  if (sc == SL_STATUS_EMPTY) {
    return;
  }

  if (sc != SL_STATUS_OK) {
    measurement_report_emit_app_error("serial_cmd",
                                      "read_failed",
                                      sc);
    return;
  }

  for (size_t i = 0u; i < bytes_read; i++) {
    process_input_byte(rx_buffer[i]);
  }
}

static void process_input_byte(char byte)
{
  if (byte == '\r' || byte == '\n') {
    if (command_length > 0u) {
      command_buffer[command_length] = '\0';
      measurement_report_emit_app_command("rx_line",
                                          command_buffer,
                                          (uint32_t)command_length);
      process_command_line(command_buffer);
      command_length = 0u;
      command_buffer[0] = '\0';
    }
    return;
  }

  if (command_length >= (SERIAL_COMMAND_BUFFER_SIZE - 1u)) {
    command_length = 0u;
    command_buffer[0] = '\0';
    measurement_report_emit_app_error("serial_cmd",
                                      "command_too_long",
                                      0u);
    return;
  }

  command_buffer[command_length] = byte;
  command_length++;
}

static void process_command_line(const char *command)
{
  sl_status_t sc;

  if (command == NULL || is_blank_line(command)) {
    return;
  }

  if (strcmp(command, "mode?") == 0) {
    emit_mode_query();
    return;
  }

  if (strcmp(command, "mode=cs") == 0) {
    sc = app_mode_request_switch(APP_MODE_CS_INITIATOR, APP_MODE_SOURCE_SERIAL);
    if (sc != SL_STATUS_OK) {
      measurement_report_emit_app_error("serial_cmd",
                                        "switch_failed",
                                        sc);
    }
    return;
  }

  if (strcmp(command, "mode=ble") == 0) {
    sc = app_mode_request_switch(APP_MODE_BEACON_SCAN, APP_MODE_SOURCE_SERIAL);
    if (sc != SL_STATUS_OK) {
      measurement_report_emit_app_error("serial_cmd",
                                        "switch_failed",
                                        sc);
    }
    return;
  }

  if (strcmp(command, "mode=default") == 0) {
    sc = app_mode_clear_override(APP_MODE_SOURCE_SERIAL);
    if (sc != SL_STATUS_OK) {
      measurement_report_emit_app_error("serial_cmd",
                                        "clear_override_failed",
                                        sc);
    }
    return;
  }

  if (strcmp(command, "help") == 0) {
    sl_iostream_printf(sl_iostream_recommended_console_stream,
                       "mode?\nmode=cs\nmode=ble\nmode=default\nhelp\n");
    return;
  }

  measurement_report_emit_app_error("serial_cmd",
                                    "unknown_command",
                                    0u);
}

static void emit_mode_query(void)
{
  const app_mode_state_t *mode_state = app_mode_get_state();
  const char *override_mode = mode_state->override_valid
                              ? app_mode_to_str(mode_state->override_mode)
                              : "none";

  measurement_report_emit_app_mode("query",
                                   app_mode_to_str(mode_state->active_mode),
                                   app_mode_to_str(mode_state->default_mode),
                                   override_mode,
                                   app_mode_source_to_str(mode_state->source));
}

static bool is_blank_line(const char *command)
{
  return (command[0] == '\0');
}
