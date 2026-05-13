#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

/*
 * Subset of sakura_firmware/sakura/can/can_protocol.h and
 * nemoto-fw/main/can/can_protocol.h. Three repos keep their own copy;
 * the values below MUST stay byte-identical across all three. A CI lint
 * (or grep-based check) should fail if they diverge.
 */

/* Admin channel IDs */
#define CAN_ADMIN_ID                        0x3F0
#define CAN_ADMIN_RESP_ID                   0x3F1

/* Admin commands the bootloader observes */
#define CAN_ADMIN_QUERY_UNASSIGNED          0x00
#define CAN_ADMIN_DEVICE_RESPONSE           0x20
#define CAN_ADMIN_ASSIGN_ACK                0x21
#define CAN_ADMIN_RESET_SHORT_ID            0x12
#define CAN_ADMIN_RESET_ALL                 0x32
#define CAN_ADMIN_ASSIGN_ID_MASK            0xF0  /* matches 0xF0..0xF3 */

/* Discovery slot stagger — must agree with sakura_firmware app & host */
#define CAN_ADMIN_DISCOVERY_SLOT_COUNT      4096u
#define CAN_ADMIN_DISCOVERY_SLOT_PITCH_MS   1u

/* Application protocol ID space (used post-assignment for Katapult) */
#define CAN_APP_RX_BASE                     0x400  /* device->host = 0x400 | short_id */
#define CAN_MAX_DEVICE_ID                   1023

/* Reserved short_ids (admin channel overlaps) */
#define CAN_RESERVED_ID_ADMIN_TX            1008   /* 0x3F0 overlap */
#define CAN_RESERVED_ID_ADMIN_RX            1009   /* 0x3F1 overlap */

/* Device type byte returned in CAN_ADMIN_DEVICE_RESPONSE.data[7] and in
 * CAN_ADMIN_ASSIGN_ACK.data[3]. Bootloader uses HW_TYPE_SAKURA_BOOT so
 * the host can tell bootloader-mode entries apart from app-mode ones. */
#define HW_TYPE_SAKURA                      0x10
#define HW_TYPE_SAKURA_BOOT                 0xB0

#endif /* CAN_PROTOCOL_H */
