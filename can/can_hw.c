#include "can_hw.h"

#include "fcm32_can.h"
#include "stm32f1xx_hal.h" /* For GPIO HAL, RCC macros, NVIC */

#define FCM32_CAN_APB1_CLK_BIT                                          \
  (1UL << 25)                         /* RCC_APB1ENR bit 25 = CAN clock \
                                       */
#define FCM32_CAN_PCLK1_HZ 36000000UL /* APB1 clock = 72MHz / 2 */

void can_hw_init(uint32_t baud_rate) {
  FCM32_CAN_InitTypeDef init;
  GPIO_InitTypeDef gpio;

  /* ── Enable clocks ─────────────────────────────────────────────── */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  RCC->APB1ENR |= FCM32_CAN_APB1_CLK_BIT;

  /* ── Configure GPIO: PA11 = CAN_RX, PA12 = CAN_TX ─────────────── */
  gpio.Pin = GPIO_PIN_11;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  gpio.Pin = GPIO_PIN_12;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* ── Enter reset mode, set normal operating mode ──────────────── */
  /* Per vendor F103 SDK: Reset ENABLE, then Normal ENABLE, then config */
  FCM32_CAN_OperatingModeRequest(FCM32_CAN, CAN_OperatingMode_Reset, ENABLE);
  FCM32_CAN_OperatingModeRequest(FCM32_CAN, CAN_OperatingMode_Normal, ENABLE);

  /* ── Configure baud rate ───────────────────────────────────────── */
  FCM32_CAN_StructInit(&init);
  FCM32_CAN_AutoCfg_Baud(&init, FCM32_CAN_PCLK1_HZ, baud_rate);
  init.CAN_EWLR = 0x96;
  FCM32_CAN_Init(FCM32_CAN, &init);

  /* ── NVIC: enable CAN interrupt vectors ────────────────────────── */
  /* FCM32 clone routes all CAN interrupts to a single hardware line.
   * Which IRQ vector it actually fires on is undocumented — enable all
   * four CAN-related vectors (19-22) so we catch it regardless. */
  HAL_NVIC_SetPriority(USB_HP_CAN1_TX_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
  HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
  HAL_NVIC_SetPriority(CAN1_RX1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(CAN1_RX1_IRQn);
  HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
}

void can_hw_start(void) {
  FCM32_CAN_OperatingModeRequest(FCM32_CAN, CAN_OperatingMode_Reset, DISABLE);
}

void can_hw_stop(void) {
  FCM32_CAN_OperatingModeRequest(FCM32_CAN, CAN_OperatingMode_Reset, ENABLE);
}

void can_hw_set_filter(uint16_t id, uint16_t mask) {
  /*
   * Standard frame, single-filter mode (SJA1000 PeliCAN).
   *
   * Acceptance mask polarity (confirmed by vendor PDF):
   *   bit = 0 → MUST MATCH acceptance code
   *   bit = 1 → DON'T CARE (accept any value)
   *
   * Single-filter standard-frame byte layout:
   *   ACR0 / AMR0  = ID[10:3]
   *   ACR1 / AMR1  = ID[2:0] | RTR | IDE | unused[2:0]
   *   ACR2 / AMR2  = Data byte 0
   *   ACR3 / AMR3  = Data byte 1
   *
   * For "accept all": pass id=0x000, mask=0x7FF.
   */
  uint32_t id_shifted = (uint32_t)id << (5 + 16);
  uint32_t mask_shifted = (uint32_t)mask << (5 + 16);

  FCM32_CAN_FilterInitTypeDef f = {0};

  /* Enable single-filter mode */
  FCM32_CAN_OperatingModeRequest(FCM32_CAN, CAN_OperatingMode_SingleFilter,
                                 ENABLE);

  /* Acceptance code (what to match against) */
  f.CAN_FilterId0 = (uint8_t)(id_shifted >> 24);
  f.CAN_FilterId1 = (uint8_t)(id_shifted >> 16);
  f.CAN_FilterId2 = 0x00;
  f.CAN_FilterId3 = 0x00;

  /* Acceptance mask (0 = must match, 1 = don't care) */
  f.CAN_FilterMaskId0 = (uint8_t)(mask_shifted >> 24);
  f.CAN_FilterMaskId1 = (uint8_t)(mask_shifted >> 16) | 0x1F; /* don't care RTR/IDE/unused */
  f.CAN_FilterMaskId2 = 0xFF;                                 /* don't care data byte 0 */
  f.CAN_FilterMaskId3 = 0xFF;                                 /* don't care data byte 1 */

  FCM32_CAN_FilterInit(FCM32_CAN, &f);
}

void can_hw_set_dual_filter(uint16_t id1, uint16_t mask1,
                            uint16_t id2, uint16_t mask2) {
  /*
   * Double-filter mode for standard frames (SJA1000 PeliCAN).
   * MOD.AFM = 0 (SingleFilter DISABLED) → two independent 16-bit filters.
   *
   * Filter 1: ACR0/AMR0 = ID[10:3], ACR1/AMR1 = ID[2:0]+RTR+data[7:4]
   * Filter 2: ACR2/AMR2 = ID[10:3], ACR3/AMR3 = ID[2:0]+RTR+data[3:0]
   *
   * Vendor ref: StdFrm_DblFlt case in main.c
   *   Filter 1: idCode << (5+16), split into bytes 0-1
   *   Filter 2: idCode << 5,      split into bytes 2-3
   */
  uint32_t code1 = (uint32_t)id1 << (5 + 16);
  uint32_t msk1 = (uint32_t)mask1 << (5 + 16);
  uint32_t code2 = (uint32_t)id2 << 5;
  uint32_t msk2 = (uint32_t)mask2 << 5;

  FCM32_CAN_FilterInitTypeDef f = {0};

  /* Disable single-filter mode → enables double-filter */
  FCM32_CAN_OperatingModeRequest(FCM32_CAN, CAN_OperatingMode_SingleFilter,
                                 DISABLE);

  /* Acceptance codes */
  f.CAN_FilterId0 = (uint8_t)(code1 >> 24);
  f.CAN_FilterId1 = (uint8_t)(code1 >> 16);
  f.CAN_FilterId2 = (uint8_t)(code2 >> 8);
  f.CAN_FilterId3 = (uint8_t)(code2 >> 0);

  /* Acceptance masks (0 = must match, 1 = don't care) */
  f.CAN_FilterMaskId0 = (uint8_t)(msk1 >> 24);
  f.CAN_FilterMaskId1 = (uint8_t)(msk1 >> 16) | 0x1F; /* don't care RTR + data nibble */
  f.CAN_FilterMaskId2 = (uint8_t)(msk2 >> 8);
  f.CAN_FilterMaskId3 = (uint8_t)(msk2 >> 0) | 0x1F;  /* don't care RTR + data nibble */

  FCM32_CAN_FilterInit(FCM32_CAN, &f);
}

bool can_hw_transmit(const CAN_HW_Message* msg) {
  FCM32_CanTxMsg tx = {0};
  tx.StdId = msg->std_id;
  tx.IDE = CAN_Id_Standard;
  tx.RTR = CAN_RTR_Data;
  tx.DLC = msg->dlc;
  tx.SS = 0;

  for (uint8_t i = 0; i < msg->dlc && i < 8; i++) tx.Data[i] = msg->data[i];

  return FCM32_CAN_Transmit(FCM32_CAN, &tx) == CAN_TxStatus_Pending;
}

bool can_hw_tx_ready(void) {
  return FCM32_CAN_GetFlagStatus(FCM32_CAN, CAN_STATUS_TBS) == SET;
}

void can_hw_cancel_transmit(void) { FCM32_CAN_CancelTransmit(FCM32_CAN); }

bool can_hw_receive(CAN_HW_Message* msg) {
  if (FCM32_CAN_MessagePending(FCM32_CAN) == 0) return false;

  FCM32_CanRxMsg rx;
  FCM32_CAN_Receive(FCM32_CAN, &rx);

  if (rx.IDE != CAN_Id_Standard) return false; /* We only use standard IDs */

  msg->std_id = rx.StdId;
  msg->dlc = rx.DLC;
  for (uint8_t i = 0; i < rx.DLC && i < 8; i++) msg->data[i] = rx.Data[i];

  return true;
}

uint8_t can_hw_rx_pending(void) { return FCM32_CAN_MessagePending(FCM32_CAN); }

void can_hw_enable_interrupts(void) {
  /* Per vendor F103 SDK: enable RI (receive) and EI (error warning).
   * EI fires on bus-off recovery too. Also enable TI for TX queue. */
  FCM32_CAN_ITConfig(FCM32_CAN, CAN_IT_RI | CAN_IT_TI | CAN_IT_EI, ENABLE);
}

void can_hw_disable_interrupts(void) {
  FCM32_CAN_ITConfig(FCM32_CAN,
                     CAN_IT_RI | CAN_IT_TI | CAN_IT_EI | CAN_IT_DOI |
                         CAN_IT_EPI | CAN_IT_BEI | CAN_IT_WUI | CAN_IT_ALI,
                     DISABLE);
}

void can_hw_get_errors(CAN_HW_ErrorInfo* info) {
  uint32_t sr;
  sr = FCM32_CAN->SR;
  sr = FCM32_CAN->SR;

  info->tx_err_count = FCM32_CAN_GetTransmitErrorCounter(FCM32_CAN);
  info->rx_err_count = FCM32_CAN_GetReceiveErrorCounter(FCM32_CAN);
  info->last_error_code = FCM32_CAN_GetLastErrorCode(FCM32_CAN);
  info->bus_off = (sr & CAN_SR_BS) != 0;
  info->error_passive =
      (info->tx_err_count >= 128) || (info->rx_err_count >= 128);
  info->error_warning = (sr & CAN_SR_ES) != 0;
  info->data_overrun = (sr & CAN_SR_DOS) != 0;
}

void can_hw_clear_data_overrun(void) { FCM32_CAN_ClearDataOverrun(FCM32_CAN); }

uint32_t can_hw_read_clear_interrupts(void) {
  /* IMPORTANT: Reading IR clears all flags EXCEPT RI.
   * Single read only — a second read would lose non-RI flags. */
  return FCM32_CAN->IR;
}
