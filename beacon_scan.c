#include "beacon_scan.h"

#include <stddef.h>
#include <string.h>
#include "app_config.h"
#include "app_timer.h"
#include "measurement_report.h"

#define AD_TYPE_MANUFACTURER_SPECIFIC_DATA 0xFFu
#define APPLE_COMPANY_ID                   0x004Cu
#define IBEACON_TYPE                       0x02u
#define IBEACON_LENGTH                     0x15u

typedef struct {
  const char *format_name;
  uint16_t company_id;
  int8_t tx_power_dbm;
  bool company_id_valid;
  bool uuid_valid;
  bool tx_power_valid;
  uint8_t uuid[16];
} beacon_payload_info_t;

static bool scanner_running = false;
static bool scanner_start_pending = false;
static bool scanner_stop_pending = false;
static app_timer_t scan_duty_cycle_timer;

static void scan_duty_cycle_timer_callback(app_timer_t *timer, void *data);
static sl_status_t start_scanner(void);
static sl_status_t stop_scanner(void);
static void parse_advertisement_data(const uint8_t *data,
                                     size_t data_len,
                                     beacon_payload_info_t *payload_info);
static void emit_beacon_scan_event(const bd_addr *address,
                                   uint8_t address_type,
                                   int8_t rssi_dbm,
                                   uint8_t channel,
                                   bool extended,
                                   const uint8_t *data,
                                   size_t data_len);

void beacon_scan_init(void)
{
  scanner_running = false;
  scanner_start_pending = false;
  scanner_stop_pending = false;
}

sl_status_t beacon_scan_handle_boot(const bd_addr *initiator_address,
                                    uint8_t address_type)
{
  sl_status_t sc = sl_bt_scanner_set_parameters(sl_bt_scanner_scan_mode_passive,
                                                BEACON_SCAN_SCAN_INTERVAL,
                                                BEACON_SCAN_SCAN_WINDOW);

  measurement_report_emit_beacon_boot(initiator_address,
                                      address_type,
                                      BEACON_SCAN_SCAN_TIME_MS,
                                      BEACON_SCAN_SLEEP_TIME_MS,
                                      BEACON_SCAN_SCAN_INTERVAL,
                                      BEACON_SCAN_SCAN_WINDOW);

  if (sc != SL_STATUS_OK) {
    measurement_report_emit_beacon_error("scanner",
                                         "set_parameters_failed",
                                         sc,
                                         0u);
    return sc;
  }

  scanner_start_pending = true;
  return SL_STATUS_OK;
}

void beacon_scan_process_action(void)
{
  sl_status_t sc;

  if (scanner_stop_pending) {
    scanner_stop_pending = false;
    sc = stop_scanner();
    if (sc != SL_STATUS_OK) {
      measurement_report_emit_beacon_error("scanner",
                                           "stop_failed",
                                           sc,
                                           0u);
    } else if (BEACON_SCAN_SLEEP_TIME_MS > 0u) {
      (void)app_timer_start(&scan_duty_cycle_timer,
                            BEACON_SCAN_SLEEP_TIME_MS,
                            scan_duty_cycle_timer_callback,
                            NULL,
                            false);
    }
  }

  if (scanner_start_pending) {
    scanner_start_pending = false;
    sc = start_scanner();
    if (sc != SL_STATUS_OK) {
      measurement_report_emit_beacon_error("scanner",
                                           "start_failed",
                                           sc,
                                           0u);
    } else if (BEACON_SCAN_SLEEP_TIME_MS > 0u) {
      (void)app_timer_start(&scan_duty_cycle_timer,
                            BEACON_SCAN_SCAN_TIME_MS,
                            scan_duty_cycle_timer_callback,
                            NULL,
                            false);
    }
  }
}

void beacon_scan_handle_legacy_report(
  const sl_bt_evt_scanner_legacy_advertisement_report_t *report)
{
  if (report == NULL) {
    return;
  }

  emit_beacon_scan_event(&report->address,
                         report->address_type,
                         report->rssi,
                         report->channel,
                         false,
                         report->data.data,
                         report->data.len);
}

void beacon_scan_handle_extended_report(
  const sl_bt_evt_scanner_extended_advertisement_report_t *report)
{
  if (report == NULL) {
    return;
  }

  emit_beacon_scan_event(&report->address,
                         report->address_type,
                         report->rssi,
                         report->channel,
                         true,
                         report->data.data,
                         report->data.len);
}

static void scan_duty_cycle_timer_callback(app_timer_t *timer, void *data)
{
  (void)timer;
  (void)data;

  if (BEACON_SCAN_SLEEP_TIME_MS == 0u) {
    return;
  }

  if (scanner_running) {
    scanner_stop_pending = true;
  } else {
    scanner_start_pending = true;
  }
}

static sl_status_t start_scanner(void)
{
  sl_status_t sc;

  if (scanner_running) {
    return SL_STATUS_OK;
  }

  sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,
                           BEACON_SCAN_DISCOVER_MODE);
  if (sc == SL_STATUS_OK) {
    scanner_running = true;
  }

  return sc;
}

static sl_status_t stop_scanner(void)
{
  sl_status_t sc;

  if (!scanner_running) {
    return SL_STATUS_OK;
  }

  sc = sl_bt_scanner_stop();
  if (sc == SL_STATUS_OK) {
    scanner_running = false;
  }

  return sc;
}

static void parse_advertisement_data(const uint8_t *data,
                                     size_t data_len,
                                     beacon_payload_info_t *payload_info)
{
  size_t offset = 0u;

  if (payload_info == NULL) {
    return;
  }

  memset(payload_info, 0, sizeof(*payload_info));
  payload_info->format_name = "unknown";

  while (data != NULL && offset < data_len) {
    uint8_t field_len = data[offset];
    size_t field_total_len = (size_t)field_len + 1u;

    if (field_len == 0u || (offset + field_total_len) > data_len) {
      break;
    }

    if (data[offset + 1u] == AD_TYPE_MANUFACTURER_SPECIFIC_DATA && field_len >= 3u) {
      const uint8_t *field_data = &data[offset + 2u];
      size_t manufacturer_len = (size_t)field_len - 1u;

      payload_info->company_id = (uint16_t)field_data[0]
                                 | ((uint16_t)field_data[1] << 8);
      payload_info->company_id_valid = true;

      if (payload_info->company_id == APPLE_COMPANY_ID
          && manufacturer_len >= 25u
          && field_data[2] == IBEACON_TYPE
          && field_data[3] == IBEACON_LENGTH) {
        payload_info->format_name = "ibeacon";
        memcpy(payload_info->uuid, &field_data[4], sizeof(payload_info->uuid));
        payload_info->uuid_valid = true;
        payload_info->tx_power_dbm = (int8_t)field_data[24];
        payload_info->tx_power_valid = true;
        return;
      }
    }

    offset += field_total_len;
  }
}

static void emit_beacon_scan_event(const bd_addr *address,
                                   uint8_t address_type,
                                   int8_t rssi_dbm,
                                   uint8_t channel,
                                   bool extended,
                                   const uint8_t *data,
                                   size_t data_len)
{
  beacon_payload_info_t payload_info;
  measurement_report_beacon_t beacon_event;

  parse_advertisement_data(data, data_len, &payload_info);

  memset(&beacon_event, 0, sizeof(beacon_event));
  if (address != NULL) {
    beacon_event.address = *address;
  }
  beacon_event.address_type = address_type;
  beacon_event.rssi_dbm = rssi_dbm;
  beacon_event.channel = channel;
  beacon_event.company_id = payload_info.company_id;
  beacon_event.tx_power_dbm = payload_info.tx_power_dbm;
  beacon_event.company_id_valid = payload_info.company_id_valid;
  beacon_event.uuid_valid = payload_info.uuid_valid;
  beacon_event.tx_power_valid = payload_info.tx_power_valid;
  beacon_event.extended = extended;
  memcpy(beacon_event.uuid, payload_info.uuid, sizeof(beacon_event.uuid));

  measurement_report_emit_beacon_scan(&beacon_event, payload_info.format_name);
}
