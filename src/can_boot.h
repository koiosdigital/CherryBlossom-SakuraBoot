#ifndef CAN_BOOT_H
#define CAN_BOOT_H

#include <stdint.h>

/* Initialize bootloader protocol state */
void can_boot_init(void);

/* Feed received CAN frame data into the reassembly buffer */
void can_boot_process_rx(const uint8_t *data, uint8_t dlc);

/* Poll TX — send pending response frames over CAN */
void can_boot_tx_poll(void);

#endif /* CAN_BOOT_H */
