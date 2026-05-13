#include "can_admin.h"

#include <string.h>

#include "can_boot.h"
#include "can_helpers.h"
#include "can_hw.h"
#include "can_protocol.h"
#include "fasthash.h"
#include "main.h"
#include "stm32f1xx_hal.h"

/* Boot-magic — shared with main.c. Bootloader writes this back into
 * RAM before NVIC_SystemReset() so the next boot lands in the
 * bootloader instead of jumping to the (still-flashed) app. */
#define REQUEST_BOOTLOADER  0x5984E3FA6CA1589BULL
extern uint64_t _boot_magic;
#define BOOT_MAGIC_ADDR     ((volatile uint64_t *)&_boot_magic)

/* Module state. Bootloader is single-threaded and polled, so no
 * volatile/atomics needed — there is no ISR path into this state. */
static struct {
  uint8_t  device_id[6];
  uint8_t  hw_type;
  uint16_t short_id;
  bool     is_assigned;

  /* Discovery response stagger. On QUERY_UNASSIGNED we compute a
   * UUID-derived slot offset and defer the response by that many ms.
   * Single-slot only — once pending=true we ignore further QUERY frames
   * until the slot fires. Matches sakura_firmware's behaviour. */
  bool     discovery_pending;
  uint32_t discovery_send_tick;
  uint8_t  discovery_response[8];

  /* Single-slot admin TX queue. ASSIGN_ACK and DEVICE_RESPONSE both
   * land here; if a discovery slot fires while an ACK is still pending
   * the ACK wins (we drop the discovery, which is fine — once we're
   * assigned, the host won't re-query us). */
  bool            tx_pending;
  CAN_HW_Message  tx_msg;
} s_state;

static void switch_to_admin_only_filter(void) {
  can_hw_stop();
  can_hw_set_filter(CAN_ADMIN_ID, 0x000);
  can_hw_start();
}

static void switch_to_assigned_filter(uint16_t short_id) {
  can_hw_stop();
  can_hw_set_dual_filter(CAN_ADMIN_ID, 0x000, short_id, 0x000);
  can_hw_start();
}

static void queue_assign_ack(uint16_t short_id) {
  s_state.tx_msg.std_id = CAN_ADMIN_RESP_ID;
  s_state.tx_msg.dlc = 5;
  s_state.tx_msg.data[0] = CAN_ADMIN_ASSIGN_ACK;
  s_state.tx_msg.data[1] = (uint8_t)(short_id & 0xFF);
  s_state.tx_msg.data[2] = (uint8_t)((short_id >> 8) & 0xFF);
  s_state.tx_msg.data[3] = s_state.hw_type;
  s_state.tx_msg.data[4] = 0x01; /* success */
  s_state.tx_pending = true;
}

void can_admin_init(uint8_t hw_type) {
  can_get_device_id(s_state.device_id);
  s_state.hw_type = hw_type;
  s_state.short_id = 0;
  s_state.is_assigned = false;
  s_state.discovery_pending = false;
  s_state.tx_pending = false;
  can_hw_set_filter(CAN_ADMIN_ID, 0x000);
}

bool can_admin_handle_frame(const CAN_HW_Message *msg) {
  if (msg->std_id != CAN_ADMIN_ID) return false;
  if (msg->dlc < 1) return true; /* still an admin frame; just drop it */

  uint8_t cmd = msg->data[0];

  switch (cmd) {
    case CAN_ADMIN_QUERY_UNASSIGNED: {
      /* Skip when assigned (the host already knows us) or when we
       * already have a discovery slot pending (re-queries from the
       * host's slow-tail loop must not push our send_tick out). */
      if (s_state.is_assigned || s_state.discovery_pending) break;

      uint32_t slot = fasthash32(s_state.device_id, 6, 0)
                    % CAN_ADMIN_DISCOVERY_SLOT_COUNT;
      s_state.discovery_send_tick =
          HAL_GetTick() + slot * CAN_ADMIN_DISCOVERY_SLOT_PITCH_MS;
      s_state.discovery_response[0] = CAN_ADMIN_DEVICE_RESPONSE;
      memcpy(&s_state.discovery_response[1], s_state.device_id, 6);
      s_state.discovery_response[7] = s_state.hw_type;
      s_state.discovery_pending = true;
      break;
    }

    case CAN_ADMIN_RESET_SHORT_ID:
      /* Forget our short_id, drop Katapult TX, restore admin-only
       * filter, and become discoverable again. The host uses this to
       * resync after losing track of its assignments. */
      s_state.short_id = 0;
      s_state.is_assigned = false;
      can_boot_set_tx_id(0);
      switch_to_admin_only_filter();
      break;

    case CAN_ADMIN_RESET_ALL:
      /* Stay in bootloader after reset — the bootloader doesn't try to
       * jump to the app if boot magic is set. Without re-arming the
       * magic, an in-flight OTA would be lost: the partially-written
       * app passes the validity check less often than not, but the
       * design rule is "RESET_ALL from inside an OTA window keeps us
       * in the bootloader". */
      *BOOT_MAGIC_ADDR = REQUEST_BOOTLOADER;
      NVIC_SystemReset();
      break;

    default:
      /* ASSIGN_ID: 0xF0..0xF3 (top six bits = 0xF0, low two bits =
       * high two bits of short_id). data[1] = low byte of short_id,
       * data[2..7] = UUID. We only accept assignments addressed to
       * our exact UUID, and only valid short_id values. */
      if ((cmd & 0xFC) == CAN_ADMIN_ASSIGN_ID_MASK && msg->dlc >= 8) {
        uint16_t short_id = (uint16_t)(((cmd & 0x03) << 8) | msg->data[1]);
        if (memcmp(&msg->data[2], s_state.device_id, 6) == 0 &&
            short_id > 0 && short_id <= CAN_MAX_DEVICE_ID &&
            short_id != CAN_RESERVED_ID_ADMIN_TX &&
            short_id != CAN_RESERVED_ID_ADMIN_RX) {
          s_state.short_id = short_id;
          s_state.is_assigned = true;
          /* Drop any pending discovery — we're addressed now. */
          s_state.discovery_pending = false;
          switch_to_assigned_filter(short_id);
          can_boot_set_tx_id(CAN_APP_RX_BASE | short_id);
          queue_assign_ack(short_id);
        }
      }
      break;
  }

  return true;
}

void can_admin_tick(void) {
  /* Promote a discovery response to the TX queue once its slot fires.
   * The cast-then-compare trick handles HAL_GetTick wraparound. */
  if (s_state.discovery_pending && !s_state.tx_pending &&
      (int32_t)(HAL_GetTick() - s_state.discovery_send_tick) >= 0) {
    s_state.tx_msg.std_id = CAN_ADMIN_RESP_ID;
    s_state.tx_msg.dlc = 8;
    memcpy(s_state.tx_msg.data, s_state.discovery_response, 8);
    s_state.tx_pending = true;
    s_state.discovery_pending = false;
  }

  /* Drain one admin TX per tick. Katapult TX (can_boot_tx_poll) shares
   * the single hardware TX buffer; both poll can_hw_tx_ready, so they
   * cooperate naturally — admin always gets priority because it's
   * checked first in the main loop. */
  if (s_state.tx_pending && can_hw_tx_ready()) {
    if (can_hw_transmit(&s_state.tx_msg)) {
      s_state.tx_pending = false;
    }
  }
}

bool can_admin_is_assigned(void) { return s_state.is_assigned; }
uint16_t can_admin_get_short_id(void) { return s_state.short_id; }
