#include "can_boot.h"
#include "can_hw.h"
#include "crc16.h"
#include "flash.h"
#include "led.h"
#include "main.h"

#include <string.h>

/* Katapult protocol constants */
#define PROTO_VERSION       0x00020000U  /* v2.0.0 — per-device OTA on short_id */

#define CMD_CONNECT         0x11
#define CMD_RX_BLOCK        0x12
#define CMD_RX_EOF          0x13
#define CMD_COMPLETE        0x15

#define RESP_ACK            0xA0
#define RESP_NACK           0xF1
#define RESP_CMD_ERROR      0xF2

#define MSG_STX1            0x01
#define MSG_STX2            0x88
#define MSG_SYNC2           0x99
#define MSG_SYNC            0x03

#define MSG_MIN             8
#define MSG_MAX             128
#define MSG_POS_LEN         3

/* Katapult TX CAN ID, set at ASSIGN_ID time by can_admin to
 * (CAN_APP_RX_BASE | short_id). Zero means "not assigned yet" — the
 * hardware filter doesn't pass any Katapult RX in that state, so we
 * shouldn't have anything to TX either, but tx_poll defensively skips
 * if this is still zero. */
static uint32_t s_katapult_tx_id = 0;

/* Application address */
#define APP_START_ADDR      0x08002000U  /* metadata page start */
#define BLOCK_SIZE          FLASH_BLOCK_SIZE

/* RX reassembly buffer */
static uint8_t rx_buf[192];
static uint8_t rx_pos;

/* TX buffer */
static uint8_t tx_buf[96];
static uint8_t tx_pos;
static uint8_t tx_max;

/* Flash state */
static uint8_t in_transfer;
static uint32_t pages_written;

/* Pending reset */
static uint8_t reset_pending;
static uint32_t reset_tick;

/*--------------------------------------------------------------------
 * Response framing
 *--------------------------------------------------------------------*/

/* Build a Katapult response frame in tx_buf.
 * Format: STX1 STX2 <cmd> <len_words> [payload words...] <crc16_lo> <crc16_hi> SYNC2 SYNC
 *
 * data[] layout (as uint32_t words, little-endian):
 *   data[0] = header word (built by this function)
 *   data[1] = acked_cmd (set by caller)
 *   data[2..n-1] = response payload (set by caller)
 *   data[n-1] = trailer word (built by this function)
 */
static void respond(uint32_t cmd_id, uint32_t *data, uint32_t word_count) {
  /* word_count includes header and trailer words */
  uint32_t payload_words = word_count - 2; /* words between header and trailer */

  /* Build header: STX1 STX2 cmd len_words (all little-endian in one uint32) */
  data[0] = (uint32_t)MSG_STX1
          | ((uint32_t)MSG_STX2 << 8)
          | ((uint32_t)cmd_id << 16)
          | ((uint32_t)payload_words << 24);

  /* CRC16 over cmd + len_words + payload (bytes 2 to end of payload) */
  uint8_t *frame = (uint8_t *)data;
  uint32_t crc_len = payload_words * 4 + 2; /* cmd(1) + len(1) + payload */
  uint16_t crc = crc16_ccitt(&frame[2], crc_len);

  /* Trailer: crc16_lo crc16_hi SYNC2 SYNC */
  data[word_count - 1] = (uint32_t)(crc & 0xFF)
                       | ((uint32_t)((crc >> 8) & 0xFF) << 8)
                       | ((uint32_t)MSG_SYNC2 << 16)
                       | ((uint32_t)MSG_SYNC << 24);

  /* Copy to TX buffer */
  uint32_t total_bytes = word_count * 4;
  if (tx_max + total_bytes > sizeof(tx_buf))
    return; /* Drop if no space */
  memcpy(&tx_buf[tx_max], data, total_bytes);
  tx_max += total_bytes;
}

static void respond_ack(uint32_t acked_cmd, uint32_t *out, uint32_t word_count) {
  out[1] = acked_cmd; /* Already little-endian on ARM */
  respond(RESP_ACK, out, word_count);
}

static void respond_error(void) {
  uint32_t out[2];
  memset(out, 0, sizeof(out));
  respond(RESP_CMD_ERROR, out, 2);
}

static void respond_nack(void) {
  uint32_t out[2];
  memset(out, 0, sizeof(out));
  respond(RESP_NACK, out, 2);
}

/*--------------------------------------------------------------------
 * Command handlers
 *--------------------------------------------------------------------*/

static void handle_connect(void) {

  uint32_t out[6];
  memset(out, 0, sizeof(out));
  out[2] = PROTO_VERSION;
  out[3] = APP_START_ADDR;
  out[4] = BLOCK_SIZE;
  respond_ack(CMD_CONNECT, out, 6);

  in_transfer = 0;
  pages_written = 0;
}

static void handle_write_block(uint8_t *frame, uint8_t frame_len) {
  in_transfer = 1;

  /* Payload starts at byte 4 (after header word) */
  uint32_t *data = (uint32_t *)frame;
  uint8_t payload_words = frame[MSG_POS_LEN];

  /* Expect 1 word (address) + BLOCK_SIZE/4 words (data) */
  if (payload_words != (BLOCK_SIZE / 4) + 1) {
    respond_error();
    return;
  }

  uint32_t block_addr = data[1];
  if (block_addr < APP_START_ADDR) {
    respond_error();
    return;
  }

  /* Reset page counter at start of transfer */
  if (block_addr == APP_START_ADDR)
    pages_written = 0;

  int ret = flash_write_block(block_addr, &data[2]);
  if (ret < 0) {
    respond_error();
    return;
  }

  /* Track pages written */
  uint32_t page_addr = block_addr & ~(BOOT_FLASH_PAGE_SIZE - 1);
  if (block_addr == page_addr)
    pages_written++;

  uint32_t out[4];
  memset(out, 0, sizeof(out));
  out[2] = block_addr;
  respond_ack(CMD_RX_BLOCK, out, 4);
}

static void handle_eof(void) {
  in_transfer = 0;

  uint32_t out[4];
  memset(out, 0, sizeof(out));
  out[2] = pages_written;
  respond_ack(CMD_RX_EOF, out, 4);

  led_activity();
}

static void handle_complete(void) {
  uint32_t out[3];
  memset(out, 0, sizeof(out));
  respond_ack(CMD_COMPLETE, out, 3);

  /* Delay reset to let TX drain */
  reset_pending = 1;
  reset_tick = HAL_GetTick() + 100;
}

/*--------------------------------------------------------------------
 * Frame parsing
 *--------------------------------------------------------------------*/

static uint8_t sync_state;
#define CF_NEED_SYNC  (1 << 0)
#define CF_NEED_VALID (1 << 1)

static void dispatch(uint8_t *buf, uint8_t msglen) {
  /* Copy to aligned buffer for uint32_t access */
  uint32_t data[MSG_MAX / 4];
  memcpy(data, buf, msglen);

  led_activity();

  uint8_t cmd = buf[2]; /* cmd byte */
  switch (cmd) {
  case CMD_CONNECT:
    handle_connect();
    break;
  case CMD_RX_BLOCK:
    handle_write_block((uint8_t *)data, msglen);
    break;
  case CMD_RX_EOF:
    handle_eof();
    break;
  case CMD_COMPLETE:
    handle_complete();
    break;
  default:
    respond_error();
    break;
  }
}

static void try_parse(void) {
  while (rx_pos >= MSG_MIN) {
    /* Check for resync -- skip bytes until we find a valid STX1+STX2 pair */
    if (sync_state & CF_NEED_SYNC) {
      uint8_t *p = rx_buf;
      uint8_t *end = rx_buf + rx_pos;
      while (p + 1 < end) {
        if (p[0] == MSG_STX1 && p[1] == MSG_STX2)
          break;
        p++;
      }
      if (p + 1 >= end) {
        /* Keep last byte if it's STX1 (could be start of next frame) */
        if (rx_pos > 0 && rx_buf[rx_pos - 1] == MSG_STX1) {
          rx_buf[0] = MSG_STX1;
          rx_pos = 1;
        } else {
          rx_pos = 0;
        }
        return;
      }
      uint8_t skip = p - rx_buf;
      if (skip > 0) {
        rx_pos -= skip;
        memmove(rx_buf, rx_buf + skip, rx_pos);
      }
      sync_state &= ~CF_NEED_SYNC;
    }

    if (rx_pos < MSG_MIN)
      return;

    /* Check header */
    if (rx_buf[0] != MSG_STX1 || rx_buf[1] != MSG_STX2)
      goto error;

    /* Message length from len_words field */
    uint8_t msglen = rx_buf[MSG_POS_LEN] * 4 + 8;
    if (msglen < MSG_MIN || msglen > MSG_MAX)
      goto error;

    /* Wait for complete message */
    if (rx_pos < msglen)
      return;

    /* Check trailer */
    if (rx_buf[msglen - 2] != MSG_SYNC2 || rx_buf[msglen - 1] != MSG_SYNC)
      goto error;

    /* Verify CRC16 */
    uint16_t msg_crc = (uint16_t)rx_buf[msglen - 4]
                     | ((uint16_t)rx_buf[msglen - 3] << 8);
    uint16_t calc_crc = crc16_ccitt(&rx_buf[2], msglen - 6);
    if (msg_crc != calc_crc)
      goto error;

    /* Valid message — dispatch */
    sync_state &= ~CF_NEED_VALID;
    dispatch(rx_buf, msglen);

    /* Remove processed message */
    rx_pos -= msglen;
    if (rx_pos > 0)
      memmove(rx_buf, rx_buf + msglen, rx_pos);
    continue;

  error:
    sync_state |= CF_NEED_SYNC;
    if (!(sync_state & CF_NEED_VALID)) {
      sync_state |= CF_NEED_VALID;
      respond_nack();
    }
    /* Drop the first byte so the CF_NEED_SYNC scan on the next pass
     * cannot re-find the same bad STX1 at rx_buf[0]. Without this, a
     * frame that arrives well-framed (intact STX1+STX2 header) but with
     * a bad CRC or trailer leaves rx_buf[0]=STX1, the scan immediately
     * matches at offset 0, and we re-validate the same bytes forever —
     * the bootloader silently locks up and never processes another
     * packet for the rest of its lifetime, including subsequent OTA
     * attempts. Dropping one byte forces forward progress; the resync
     * scan picks up at the next genuine frame boundary (or empties the
     * buffer if there isn't one). */
    if (rx_pos > 0) {
      rx_pos -= 1;
      memmove(rx_buf, rx_buf + 1, rx_pos);
    }
  }
}

/*--------------------------------------------------------------------
 * Public API
 *--------------------------------------------------------------------*/

void can_boot_init(void) {
  rx_pos = 0;
  tx_pos = 0;
  tx_max = 0;
  in_transfer = 0;
  pages_written = 0;
  sync_state = 0;
  reset_pending = 0;
}

void can_boot_process_rx(const uint8_t *data, uint8_t dlc) {
  /* Append CAN frame data to reassembly buffer */
  if (rx_pos + dlc > sizeof(rx_buf)) {
    /* Buffer overflow — reset */
    rx_pos = 0;
    sync_state |= CF_NEED_SYNC;
    return;
  }
  memcpy(&rx_buf[rx_pos], data, dlc);
  rx_pos += dlc;

  try_parse();
}

void can_boot_set_tx_id(uint32_t can_id) { s_katapult_tx_id = can_id; }

void can_boot_tx_poll(void) {
  /* Send pending TX data as 8-byte CAN frames on the assigned Katapult
   * TX ID (0x400 | short_id). Skip silently if not yet assigned — the
   * filter shouldn't have passed any Katapult RX to us either, so the
   * tx_buf should be empty in that state. */
  if (s_katapult_tx_id == 0)
    return;

  while (tx_pos < tx_max) {
    if (!can_hw_tx_ready())
      return;

    CAN_HW_Message msg;
    msg.std_id = s_katapult_tx_id;
    uint8_t avail = tx_max - tx_pos;
    msg.dlc = (avail > 8) ? 8 : avail;
    memcpy(msg.data, &tx_buf[tx_pos], msg.dlc);

    if (!can_hw_transmit(&msg))
      return;

    tx_pos += msg.dlc;
  }

  /* Reset TX buffer when drained */
  if (tx_pos >= tx_max) {
    tx_pos = 0;
    tx_max = 0;
  }

  /* Handle pending reset after TX drains */
  if (reset_pending && tx_pos >= tx_max) {
    if (HAL_GetTick() >= reset_tick) {
      NVIC_SystemReset();
    }
  }
}
