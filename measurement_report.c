#include "measurement_report.h"

#include <stdio.h>
#include <string.h>
#include "app_config.h"
#include "sl_iostream.h"
#include "sl_iostream_handles.h"

#ifndef CS_INITIATOR_REPORT_QUEUE_SIZE
#define CS_INITIATOR_REPORT_QUEUE_SIZE 16u
#endif

#ifndef CS_INITIATOR_REPORT_PREFIX
#define CS_INITIATOR_REPORT_PREFIX "CS_EVT"
#endif

static measurement_report_measurement_t measurement_queue[CS_INITIATOR_REPORT_QUEUE_SIZE];
static uint8_t measurement_queue_head = 0u;
static uint8_t measurement_queue_tail = 0u;
static uint8_t measurement_queue_count = 0u;
static uint32_t dropped_measurement_count = 0u;
static uint32_t report_sequence_number = 0u;

static uint32_t next_report_sequence(void);
static void format_address(const bd_addr *address, char *buffer, size_t buffer_len);
static const char *mode_to_str(uint8_t mode);
static const char *sub_mode_to_str(uint8_t sub_mode);
static const char *address_type_to_str(uint8_t address_type);

void measurement_report_init(void)
{
  memset(measurement_queue, 0, sizeof(measurement_queue));
  measurement_queue_head = 0u;
  measurement_queue_tail = 0u;
  measurement_queue_count = 0u;
  dropped_measurement_count = 0u;
  report_sequence_number = 0u;
}

void measurement_report_emit_boot(const bd_addr *initiator_address,
                                  uint8_t address_type,
                                  uint8_t max_connections,
                                  uint8_t algo_mode,
                                  uint8_t main_mode,
                                  uint8_t sub_mode,
                                  uint16_t connection_interval,
                                  uint16_t procedure_interval,
                                  uint8_t channel_map_preset)
{
  char address_buffer[18];
  format_address(initiator_address, address_buffer, sizeof(address_buffer));

  sl_iostream_printf(sl_iostream_recommended_console_stream,
                     CS_INITIATOR_REPORT_PREFIX
                     "|type=BOOT|seq=%lu|addr=%s|addr_type=%s|max_conn=%u|algo_mode=%u|main_mode=%s|sub_mode=%s|conn_interval=%u|proc_interval=%u|channel_map=%u|scan=started\n",
                     (unsigned long)next_report_sequence(),
                     address_buffer,
                     address_type_to_str(address_type),
                     max_connections,
                     algo_mode,
                     mode_to_str(main_mode),
                     sub_mode_to_str(sub_mode),
                     connection_interval,
                     procedure_interval,
                     channel_map_preset);
}

void measurement_report_emit_anchor_up(const char *stage,
                                       uint8_t conn_handle,
                                       const bd_addr *reflector_address)
{
  char address_buffer[18];
  format_address(reflector_address, address_buffer, sizeof(address_buffer));

  sl_iostream_printf(sl_iostream_recommended_console_stream,
                     CS_INITIATOR_REPORT_PREFIX
                     "|type=ANCHOR_UP|seq=%lu|stage=%s|conn=%u|addr=%s\n",
                     (unsigned long)next_report_sequence(),
                     stage,
                     conn_handle,
                     address_buffer);
}

void measurement_report_emit_anchor_down(const char *reason,
                                         uint8_t conn_handle,
                                         const bd_addr *reflector_address)
{
  char address_buffer[18];
  format_address(reflector_address, address_buffer, sizeof(address_buffer));

  sl_iostream_printf(sl_iostream_recommended_console_stream,
                     CS_INITIATOR_REPORT_PREFIX
                     "|type=ANCHOR_DOWN|seq=%lu|reason=%s|conn=%u|addr=%s\n",
                     (unsigned long)next_report_sequence(),
                     reason,
                     conn_handle,
                     address_buffer);
}

void measurement_report_emit_error(const char *source,
                                   const char *detail,
                                   uint8_t conn_handle,
                                   const bd_addr *reflector_address,
                                   uint32_t code)
{
  char address_buffer[18];
  format_address(reflector_address, address_buffer, sizeof(address_buffer));

  sl_iostream_printf(sl_iostream_recommended_console_stream,
                     CS_INITIATOR_REPORT_PREFIX
                     "|type=ERROR|seq=%lu|source=%s|detail=%s|conn=%u|addr=%s|code=0x%08lx\n",
                     (unsigned long)next_report_sequence(),
                     source,
                     detail,
                     conn_handle,
                     address_buffer,
                     (unsigned long)code);
}

bool measurement_report_queue_push(const measurement_report_measurement_t *measurement)
{
  if (measurement == NULL) {
    return false;
  }

  if (measurement_queue_count >= CS_INITIATOR_REPORT_QUEUE_SIZE) {
    dropped_measurement_count++;
    return false;
  }

  measurement_queue[measurement_queue_head] = *measurement;
  measurement_queue_head = (measurement_queue_head + 1u) % CS_INITIATOR_REPORT_QUEUE_SIZE;
  measurement_queue_count++;
  return true;
}

bool measurement_report_queue_pop(measurement_report_measurement_t *measurement)
{
  if (measurement == NULL || measurement_queue_count == 0u) {
    return false;
  }

  *measurement = measurement_queue[measurement_queue_tail];
  measurement_queue_tail = (measurement_queue_tail + 1u) % CS_INITIATOR_REPORT_QUEUE_SIZE;
  measurement_queue_count--;
  return true;
}

void measurement_report_emit_measurement(const measurement_report_measurement_t *measurement)
{
  char address_buffer[18];

  if (measurement == NULL) {
    return;
  }

  format_address(&measurement->reflector_address, address_buffer, sizeof(address_buffer));

  sl_iostream_printf(sl_iostream_recommended_console_stream,
                     CS_INITIATOR_REPORT_PREFIX
                     "|type=MEAS|seq=%lu|conn=%u|addr=%s|meas=%lu|range_ctr=%u|dist=%.3f|raw=%.3f|like=%.3f|rssi_dist=%.3f|velocity_valid=%u|velocity=%.3f|ber_valid=%u|ber=%.3f|valid=%u\n",
                     (unsigned long)next_report_sequence(),
                     measurement->conn_handle,
                     address_buffer,
                     (unsigned long)measurement->measurement_count,
                     measurement->ranging_counter,
                     measurement->distance_filtered,
                     measurement->distance_raw,
                     measurement->likeliness,
                     measurement->distance_rssi,
                     measurement->velocity_valid ? 1u : 0u,
                     measurement->velocity,
                     measurement->bit_error_rate_valid ? 1u : 0u,
                     measurement->bit_error_rate,
                     measurement->valid ? 1u : 0u);
}

uint32_t measurement_report_take_dropped_count(void)
{
  uint32_t count = dropped_measurement_count;
  dropped_measurement_count = 0u;
  return count;
}

static uint32_t next_report_sequence(void)
{
  report_sequence_number++;
  return report_sequence_number;
}

static void format_address(const bd_addr *address, char *buffer, size_t buffer_len)
{
  if (address == NULL) {
    (void)snprintf(buffer, buffer_len, "00:00:00:00:00:00");
    return;
  }

  (void)snprintf(buffer,
                 buffer_len,
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 address->addr[5],
                 address->addr[4],
                 address->addr[3],
                 address->addr[2],
                 address->addr[1],
                 address->addr[0]);
}

static const char *mode_to_str(uint8_t mode)
{
  return (mode == sl_bt_cs_mode_pbr) ? "pbr" : "rtt";
}

static const char *sub_mode_to_str(uint8_t sub_mode)
{
  return (sub_mode == sl_bt_cs_submode_disabled) ? "disabled" : "rtt";
}

static const char *address_type_to_str(uint8_t address_type)
{
  return address_type ? "static_random" : "public";
}

