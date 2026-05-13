#ifndef CAN_BOOT_H
#define CAN_BOOT_H

#include <stdint.h>

/* Initialize bootloader protocol state */
void can_boot_init(void);

/* Set the CAN ID the Katapult TX path uses for outbound frames. Called
 * from can_admin after ASSIGN_ID succeeds, with (CAN_APP_RX_BASE |
 * short_id). Passing 0 stops TX (used on RESET_SHORT_ID). */
void can_boot_set_tx_id(uint32_t can_id);

/* Feed received CAN frame data into the reassembly buffer */
void can_boot_process_rx(const uint8_t *data, uint8_t dlc);

/* Poll TX — send pending response frames over CAN */
void can_boot_tx_poll(void);

#endif /* CAN_BOOT_H */
