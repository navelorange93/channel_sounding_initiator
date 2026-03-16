#ifndef BEACON_SCAN_H
#define BEACON_SCAN_H

#include "sl_bt_api.h"
#include "sl_status.h"

/**************************************************************************//**
 * Initialize the beacon scan mode state.
 *****************************************************************************/
void beacon_scan_init(void);

/**************************************************************************//**
 * Start beacon scanning after boot.
 *****************************************************************************/
sl_status_t beacon_scan_handle_boot(const bd_addr *initiator_address,
                                    uint8_t address_type);

/**************************************************************************//**
 * Process deferred scanner start/stop actions.
 *****************************************************************************/
void beacon_scan_process_action(void);

/**************************************************************************//**
 * Handle a legacy advertisement report.
 *****************************************************************************/
void beacon_scan_handle_legacy_report(
  const sl_bt_evt_scanner_legacy_advertisement_report_t *report);

/**************************************************************************//**
 * Handle an extended advertisement report.
 *****************************************************************************/
void beacon_scan_handle_extended_report(
  const sl_bt_evt_scanner_extended_advertisement_report_t *report);

#endif // BEACON_SCAN_H
