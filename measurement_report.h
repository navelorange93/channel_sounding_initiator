#ifndef MEASUREMENT_REPORT_H
#define MEASUREMENT_REPORT_H

#include <stdbool.h>
#include <stdint.h>
#include "sl_bt_api.h"

typedef struct {
  bd_addr reflector_address;
  uint8_t conn_handle;
  uint32_t measurement_count;
  uint16_t ranging_counter;
  float distance_filtered;
  float distance_raw;
  float likeliness;
  float distance_rssi;
  int8_t rssi_dbm;
  float velocity;
  float bit_error_rate;
  bool rssi_dbm_valid;
  bool velocity_valid;
  bool bit_error_rate_valid;
  bool valid;
} measurement_report_measurement_t;

typedef struct {
  bd_addr address;
  uint8_t address_type;
  int8_t rssi_dbm;
  uint8_t channel;
  uint16_t company_id;
  int8_t tx_power_dbm;
  bool company_id_valid;
  bool uuid_valid;
  bool tx_power_valid;
  bool extended;
  uint8_t uuid[16];
} measurement_report_beacon_t;

/**************************************************************************//**
 * Initialize the reporting module state.
 *****************************************************************************/
void measurement_report_init(void);

/**************************************************************************//**
 * Emit a structured boot record.
 *****************************************************************************/
void measurement_report_emit_boot(const bd_addr *initiator_address,
                                  uint8_t address_type,
                                  uint8_t max_connections,
                                  uint8_t algo_mode,
                                  uint8_t main_mode,
                                  uint8_t sub_mode,
                                  uint16_t connection_interval,
                                  uint16_t procedure_interval,
                                  uint8_t channel_map_preset);

/**************************************************************************//**
 * Emit a structured beacon scanner boot record.
 *****************************************************************************/
void measurement_report_emit_beacon_boot(const bd_addr *initiator_address,
                                         uint8_t address_type,
                                         uint32_t scan_time_ms,
                                         uint32_t sleep_time_ms,
                                         uint16_t scan_interval,
                                         uint16_t scan_window);

/**************************************************************************//**
 * Emit a structured reflector lifecycle record.
 *****************************************************************************/
void measurement_report_emit_anchor_up(const char *stage,
                                       uint8_t conn_handle,
                                       const bd_addr *reflector_address);

/**************************************************************************//**
 * Emit a structured reflector disconnect record.
 *****************************************************************************/
void measurement_report_emit_anchor_down(const char *reason,
                                         uint8_t conn_handle,
                                         const bd_addr *reflector_address);

/**************************************************************************//**
 * Emit a structured error record.
 *****************************************************************************/
void measurement_report_emit_error(const char *source,
                                   const char *detail,
                                   uint8_t conn_handle,
                                   const bd_addr *reflector_address,
                                   uint32_t code,
                                   uint32_t count);

/**************************************************************************//**
 * Emit a structured beacon scanner error record.
 *****************************************************************************/
void measurement_report_emit_beacon_error(const char *source,
                                          const char *detail,
                                          uint32_t code,
                                          uint32_t count);

/**************************************************************************//**
 * Queue a measurement record for deferred reporting.
 *
 * @return true if queued successfully, false if the queue is full.
 *****************************************************************************/
bool measurement_report_queue_push(const measurement_report_measurement_t *measurement);

/**************************************************************************//**
 * Pop the oldest queued measurement record.
 *
 * @return true if a record was popped, false if the queue is empty.
 *****************************************************************************/
bool measurement_report_queue_pop(measurement_report_measurement_t *measurement);

/**************************************************************************//**
 * Emit a structured measurement record immediately.
 *****************************************************************************/
void measurement_report_emit_measurement(const measurement_report_measurement_t *measurement);

/**************************************************************************//**
 * Emit a structured beacon scan record immediately.
 *****************************************************************************/
void measurement_report_emit_beacon_scan(const measurement_report_beacon_t *beacon,
                                         const char *format_name);

/**************************************************************************//**
 * Return and clear the count of dropped queued measurements.
 *****************************************************************************/
uint32_t measurement_report_take_dropped_count(void);

#endif // MEASUREMENT_REPORT_H
