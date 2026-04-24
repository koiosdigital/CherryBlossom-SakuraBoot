#ifndef CAN_HW_H
#define CAN_HW_H

#include <stdbool.h>
#include <stdint.h>

/* Unified CAN message (TX and RX) */
typedef struct {
  uint32_t std_id;  /* 11-bit standard CAN ID */
  uint8_t  dlc;     /* Data length code (0-8) */
  uint8_t  data[8];
} CAN_HW_Message;

/* Initialize CAN peripheral: clocks, GPIO PA11/PA12, baud rate, debug freeze.
 * Leaves CAN in reset mode — call can_hw_start() to go live. */
void can_hw_init(uint32_t baud_rate);

/* Exit reset mode — CAN begins participating on the bus */
void can_hw_start(void);

/* Enter reset mode — CAN stops bus activity */
void can_hw_stop(void);

/* Configure single acceptance filter for standard 11-bit IDs.
 * Must be called while in reset mode.
 * id/mask: 11-bit values. Mask: 0 = must match, 1 = don't care. */
void can_hw_set_filter(uint16_t id, uint16_t mask);

/* Configure dual acceptance filter for standard 11-bit IDs.
 * Must be called while in reset mode.
 * A message passes if it matches EITHER filter (logical OR).
 * id/mask: 11-bit values. Mask: 0 = must match, 1 = don't care. */
void can_hw_set_dual_filter(uint16_t id1, uint16_t mask1,
                            uint16_t id2, uint16_t mask2);

/* Transmit a standard-ID message. Returns true if accepted by HW buffer. */
bool can_hw_transmit(const CAN_HW_Message *msg);

/* Check if TX buffer is available */
bool can_hw_tx_ready(void);

/* Cancel any pending transmission */
void can_hw_cancel_transmit(void);

/* Receive a message. Returns true if a message was read. */
bool can_hw_receive(CAN_HW_Message *msg);

/* Number of messages pending in RX FIFO */
uint8_t can_hw_rx_pending(void);

/* Enable CAN interrupts (RI, TI, EI, DOI, EPI, BEI) */
void can_hw_enable_interrupts(void);

/* Disable all CAN interrupts */
void can_hw_disable_interrupts(void);

/* Error information snapshot */
typedef struct {
  uint8_t tx_err_count;
  uint8_t rx_err_count;
  uint8_t last_error_code;
  bool    bus_off;
  bool    error_passive;
  bool    error_warning;
  bool    data_overrun;
} CAN_HW_ErrorInfo;

/* Read current error state (non-destructive) */
void can_hw_get_errors(CAN_HW_ErrorInfo *info);

/* Clear data overrun condition */
void can_hw_clear_data_overrun(void);

/* Read and clear the interrupt register. Returns raw IR value. */
uint32_t can_hw_read_clear_interrupts(void);

#endif /* CAN_HW_H */
