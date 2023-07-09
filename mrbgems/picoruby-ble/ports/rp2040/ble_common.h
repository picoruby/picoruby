#ifndef BLE_COMMON_DEFINED_H_
#define BLE_COMMON_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Globals used in ports/rp2040/ble_{peripheral,central}.c */
extern hci_con_handle_t con_handle;
extern btstack_timer_source_t ble_heartbeat;
extern btstack_packet_callback_registration_t ble_hci_event_callback_registration;
void BLE_heartbeat_handler(struct btstack_timer_source *ts);


#ifdef __cplusplus
}
#endif

#endif /* BLE_COMMON_DEFINED_H_ */

