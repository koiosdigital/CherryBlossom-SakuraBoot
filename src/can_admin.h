#ifndef CAN_ADMIN_H
#define CAN_ADMIN_H

#include <stdbool.h>
#include <stdint.h>

#include "can_hw.h"

/* Minimal admin protocol handler for sakura_boot — implements the same
 * QUERY_UNASSIGNED / ASSIGN_ID handshake the application firmware uses.
 *
 * Lifecycle:
 *   1. can_admin_init(HW_TYPE_SAKURA_BOOT) after can_hw_start(). The
 *      bootloader powers up with an admin-only CAN filter.
 *   2. Host sends QUERY_UNASSIGNED → admin schedules a UUID-staggered
 *      DEVICE_RESPONSE.
 *   3. Host sends ASSIGN_ID with matching UUID → admin stores the
 *      short_id, switches the CAN filter to admin+short_id, hands the
 *      Katapult TX ID (0x400|short_id) to can_boot, and emits an
 *      ASSIGN_ACK on CAN_ADMIN_RESP_ID.
 *   4. Host sends Katapult on `short_id`; can_boot replies on the
 *      hand-off TX ID.
 *
 * Both the RX dispatcher and the periodic tick run from the main loop
 * (the bootloader is polled, never interrupt-driven for CAN). */

/* Initialise admin state. Reads the UUID, stores hw_type, sets the
 * initial admin-only filter, and arms can_hw for RX. */
void can_admin_init(uint8_t hw_type);

/* Feed an RX frame into the admin handler. Returns true if the frame
 * was an admin frame (CAN_ADMIN_ID) and was consumed — main loop should
 * then NOT pass it to can_boot. False means "not for me, route it to
 * Katapult if it matches your short_id". */
bool can_admin_handle_frame(const CAN_HW_Message *msg);

/* Periodic work: dispatch a pending discovery response when its slot
 * fires, and drain any queued admin TX onto the CAN bus. Call once per
 * main-loop iteration. */
void can_admin_tick(void);

/* Test the current assignment state. */
bool can_admin_is_assigned(void);
uint16_t can_admin_get_short_id(void);

#endif /* CAN_ADMIN_H */
