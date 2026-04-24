/**
  ******************************************************************************
  * @file    stm32f1xx_hal_can.c
  * @brief   HAL CAN driver for FCM32 PeliCAN-compatible CAN peripheral.
  *
  *          The FCM32 CAN IP is SJA1000/PeliCAN-compatible:
  *            - Single TX buffer (not 3 mailboxes)
  *            - Single RX FIFO with message counter (not 2 FIFOs)
  *            - Simple 4-byte acceptance filter
  *            - Registers must be read twice (hardware requirement)
  *            - Internal /2 clock divider: CAN_clk = PCLK1 / 2
  ******************************************************************************
  */

#include "stm32f1xx_hal.h"

#ifdef HAL_CAN_MODULE_ENABLED

/* ── Helpers: FCM registers must be read twice ─────────────────────────────── */

static inline uint32_t fcm_read(volatile uint32_t *reg)
{
  (void)*reg;
  return *reg;
}

/*
 * Double-filter state for standard frames.
 *
 * FCM PeliCAN double-filter mode (AFM=0) provides two independent 11-bit
 * ID+mask acceptance filters packed into 8 bytes (4 ACR + 4 AMR):
 *
 *   Filter 1: ACR0 = ID1[10:3]
 *             ACR1[7:5] = ID1[2:0], ACR1[4] = RTR, ACR1[3:0] = data nibble
 *   Filter 2: ACR2 = ID2[10:3]
 *             ACR3[7:5] = ID2[2:0], ACR3[4] = RTR, ACR3[3:0] = data nibble
 *
 * AMR convention: bit=0 → must match, bit=1 → don't care.
 * bxCAN convention (used by app layer): bit=1 → must match, bit=0 → don't care.
 * Conversion: AMR_id_bits = ~bxcan_mask.
 *
 * Both filters share one register set, so we store both and reprogram
 * all 8 bytes whenever either bank is updated.
 */
static struct {
  uint32_t id;     /* 11-bit std ID to match */
  uint32_t mask;   /* 11-bit mask, bxCAN convention (1=must match) */
  uint8_t  active; /* filter enabled */
  uint8_t  fifo;   /* logical FIFO assignment (for SW routing) */
} sw_filters[2];

/* ── Init / DeInit ─────────────────────────────────────────────────────────── */

HAL_StatusTypeDef HAL_CAN_Init(CAN_HandleTypeDef *hcan)
{
  CAN_TypeDef *can = hcan->Instance;
  uint32_t mod;

  if (hcan->State == HAL_CAN_STATE_RESET)
  {
    HAL_CAN_MspInit(hcan);
  }

  /* Enter reset mode */
  mod = fcm_read(&can->MOD);
  can->MOD = mod | CAN_MOD_RM;

  /* Set operating mode bits */
  mod = fcm_read(&can->MOD);
  mod &= ~(CAN_MOD_LOM | CAN_MOD_STM);
  if (hcan->Init.Mode & 0x02U) mod |= CAN_MOD_LOM;  /* Silent = listen-only */
  if (hcan->Init.Mode & 0x04U) mod |= CAN_MOD_STM;  /* Loopback = self-test */
  can->MOD = mod;

  /* Bus timing */
  can->BTR0 = ((hcan->Init.SyncJumpWidth & 0x03U) << 6)
            | (hcan->Init.Prescaler & 0x3FU);
  can->BTR1 = ((hcan->Init.TimeSeg2 & 0x07U) << 4)
            | (hcan->Init.TimeSeg1 & 0x0FU);

  /* Error warning limit — set to 0xFF for auto bus-off recovery */
  can->EWLR = (hcan->Init.AutoBusOff == ENABLE) ? 0xFFU : 0x96U;

  /* Clear SW filter state */
  for (int i = 0; i < 2; i++)
  {
    sw_filters[i].id = 0;
    sw_filters[i].mask = 0;
    sw_filters[i].active = 0;
    sw_filters[i].fifo = 0;
  }

  hcan->ErrorCode = HAL_CAN_ERROR_NONE;
  hcan->State = HAL_CAN_STATE_READY;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_DeInit(CAN_HandleTypeDef *hcan)
{
  CAN_TypeDef *can = hcan->Instance;
  uint32_t mod = fcm_read(&can->MOD);
  can->MOD = mod | CAN_MOD_RM;

  HAL_CAN_MspDeInit(hcan);

  hcan->State = HAL_CAN_STATE_RESET;
  hcan->ErrorCode = HAL_CAN_ERROR_NONE;
  return HAL_OK;
}

__weak void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)  { UNUSED(hcan); }
__weak void HAL_CAN_MspDeInit(CAN_HandleTypeDef *hcan) { UNUSED(hcan); }

/* ── Filter configuration ──────────────────────────────────────────────────── */

/*
 * Compute ACR+AMR byte pair for one double-filter slot (std frame).
 * Returns { acr_byte, amr_byte } for the ID[10:3] byte, and
 *         { acr_byte2, amr_byte2 } for the ID[2:0]+RTR+data byte.
 */
static void filter_to_bytes(uint32_t id, uint32_t bxcan_mask, uint8_t active,
                            uint8_t *acr_hi, uint8_t *amr_hi,
                            uint8_t *acr_lo, uint8_t *amr_lo)
{
  if (!active)
  {
    /* Disabled filter → accept nothing.  Set ACR to impossible value
       with AMR = 0 (all must match) so nothing passes. */
    *acr_hi = 0xFFU;
    *amr_hi = 0x00U;
    *acr_lo = 0xFFU;
    *amr_lo = 0x00U;
    return;
  }

  /* Convert bxCAN mask (1=must match) → SJA1000 AMR (1=don't care) */
  uint32_t amr_bits = ~bxcan_mask & 0x7FFU;

  *acr_hi = (uint8_t)((id >> 3) & 0xFFU);
  *amr_hi = (uint8_t)((amr_bits >> 3) & 0xFFU);

  /* Lower byte: ID[2:0] in bits[7:5], RTR in bit[4], data nibble in [3:0] */
  *acr_lo = (uint8_t)((id & 0x07U) << 5);        /* ID[2:0] */
  *amr_lo = (uint8_t)(((amr_bits & 0x07U) << 5)   /* ID[2:0] mask */
            | 0x1FU);                               /* don't care RTR + data */
}

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan,
                                       const CAN_FilterTypeDef *sFilterConfig)
{
  CAN_TypeDef *can = hcan->Instance;
  uint32_t bank = sFilterConfig->FilterBank;

  if (bank > 1) return HAL_ERROR;

  /* Update SW filter state for this bank */
  sw_filters[bank].id     = (sFilterConfig->FilterIdHigh >> 5) & 0x7FFU;
  sw_filters[bank].mask   = (sFilterConfig->FilterMaskIdHigh >> 5) & 0x7FFU;
  sw_filters[bank].active = (sFilterConfig->FilterActivation == CAN_FILTER_ENABLE);
  sw_filters[bank].fifo   = sFilterConfig->FilterFIFOAssignment;

  /* Reprogram HW filter registers (both banks together — they share one
     register set).  Must be done in reset mode. */
  uint32_t mod = fcm_read(&can->MOD);
  uint32_t was_running = !(mod & CAN_MOD_RM);

  if (was_running)
    can->MOD = mod | CAN_MOD_RM;

  /* Double-filter mode: AFM = 0 */
  mod = fcm_read(&can->MOD);
  mod &= ~CAN_MOD_AFM;
  can->MOD = mod;

  /* Build the 8 filter bytes from both banks */
  uint8_t acr0, amr0, acr1, amr1;  /* filter 1 (bank 0) */
  uint8_t acr2, amr2, acr3, amr3;  /* filter 2 (bank 1) */

  filter_to_bytes(sw_filters[0].id, sw_filters[0].mask, sw_filters[0].active,
                  &acr0, &amr0, &acr1, &amr1);
  filter_to_bytes(sw_filters[1].id, sw_filters[1].mask, sw_filters[1].active,
                  &acr2, &amr2, &acr3, &amr3);

  can->FF    = acr0;   /* ACR0 — filter 1 ID[10:3] */
  can->ID0   = acr1;   /* ACR1 — filter 1 ID[2:0]+RTR+data */
  can->ID1   = acr2;   /* ACR2 — filter 2 ID[10:3] */
  can->DATA0 = acr3;   /* ACR3 — filter 2 ID[2:0]+RTR+data */
  can->DATA1 = amr0;   /* AMR0 — filter 1 mask */
  can->DATA2 = amr1;   /* AMR1 — filter 1 mask */
  can->DATA3 = amr2;   /* AMR2 — filter 2 mask */
  can->DATA4 = amr3;   /* AMR3 — filter 2 mask */

  if (was_running)
  {
    mod = fcm_read(&can->MOD);
    can->MOD = mod & ~CAN_MOD_RM;
  }

  return HAL_OK;
}

/* ── Start / Stop ──────────────────────────────────────────────────────────── */

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan)
{
  CAN_TypeDef *can = hcan->Instance;
  uint32_t mod = fcm_read(&can->MOD);
  can->MOD = mod & ~CAN_MOD_RM;  /* Leave reset mode → normal operation */
  hcan->State = HAL_CAN_STATE_LISTENING;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_Stop(CAN_HandleTypeDef *hcan)
{
  CAN_TypeDef *can = hcan->Instance;
  uint32_t mod = fcm_read(&can->MOD);
  can->MOD = mod | CAN_MOD_RM;   /* Enter reset mode */
  hcan->State = HAL_CAN_STATE_READY;
  return HAL_OK;
}

/* ── TX ────────────────────────────────────────────────────────────────────── */

uint32_t HAL_CAN_GetTxMailboxesFreeLevel(const CAN_HandleTypeDef *hcan)
{
  uint32_t sr = fcm_read(&hcan->Instance->SR);
  return (sr & CAN_SR_TBS) ? 1U : 0U;
}

uint32_t HAL_CAN_IsTxMessagePending(const CAN_HandleTypeDef *hcan,
                                    uint32_t TxMailboxes)
{
  (void)TxMailboxes;
  uint32_t sr = fcm_read(&hcan->Instance->SR);
  return (sr & CAN_SR_TBS) ? 0U : 1U;
}

HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan,
                                       const CAN_TxHeaderTypeDef *pHeader,
                                       const uint8_t aData[],
                                       uint32_t *pTxMailbox)
{
  CAN_TypeDef *can = hcan->Instance;
  uint32_t sr = fcm_read(&can->SR);

  if (!(sr & CAN_SR_TBS))
    return HAL_ERROR;  /* TX buffer busy */

  if (pTxMailbox)
    *pTxMailbox = CAN_TX_MAILBOX0;

  if (pHeader->IDE == CAN_ID_EXT)
  {
    /* Extended frame */
    uint32_t id_shifted = pHeader->ExtId << 3;
    can->FF = CAN_FF_FF | (pHeader->RTR & CAN_FF_RTR) | (pHeader->DLC & 0x0FU);
    can->ID0   = (uint8_t)(id_shifted >> 24);
    can->ID1   = (uint8_t)(id_shifted >> 16);
    can->DATA0 = (uint8_t)(id_shifted >> 8);
    can->DATA1 = (uint8_t)(id_shifted);
    if (!(pHeader->RTR & CAN_FF_RTR))
    {
      can->DATA2 = aData[0]; can->DATA3 = aData[1];
      can->DATA4 = aData[2]; can->DATA5 = aData[3];
      can->DATA6 = aData[4]; can->DATA7 = aData[5];
      can->DATA8 = aData[6]; can->DATA9 = aData[7];
    }
  }
  else
  {
    /* Standard frame */
    uint32_t id_shifted = pHeader->StdId << 5;
    can->FF = (pHeader->RTR & CAN_FF_RTR) | (pHeader->DLC & 0x0FU);
    can->ID0 = (uint8_t)(id_shifted >> 8);
    can->ID1 = (uint8_t)(id_shifted);
    if (!(pHeader->RTR & CAN_FF_RTR))
    {
      can->DATA0 = aData[0]; can->DATA1 = aData[1];
      can->DATA2 = aData[2]; can->DATA3 = aData[3];
      can->DATA4 = aData[4]; can->DATA5 = aData[5];
      can->DATA6 = aData[6]; can->DATA7 = aData[7];
    }
  }

  /* Issue transmit request */
  uint32_t mod = fcm_read(&can->MOD);
  if (mod & CAN_MOD_STM)
    can->CMR = CAN_CMR_SRR | CAN_CMR_AT;  /* Self-test: self-reception */
  else
    can->CMR = CAN_CMR_TR;

  return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *hcan,
                                         uint32_t TxMailboxes)
{
  (void)TxMailboxes;
  hcan->Instance->CMR = CAN_CMR_AT;
  return HAL_OK;
}

uint32_t HAL_CAN_GetTxTimestamp(const CAN_HandleTypeDef *hcan, uint32_t TxMailbox)
{
  (void)hcan; (void)TxMailbox;
  return 0U;
}

/* ── RX ────────────────────────────────────────────────────────────────────── */

uint32_t HAL_CAN_GetRxFifoFillLevel(const CAN_HandleTypeDef *hcan,
                                    uint32_t RxFifo)
{
  (void)RxFifo;
  uint32_t rmc = fcm_read(&hcan->Instance->RMC);
  return rmc & 0xFFU;
}

HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *hcan,
                                       uint32_t RxFifo,
                                       CAN_RxHeaderTypeDef *pHeader,
                                       uint8_t aData[])
{
  (void)RxFifo;
  CAN_TypeDef *can = hcan->Instance;

  uint32_t sr = fcm_read(&can->SR);
  if (!(sr & CAN_SR_RBS))
    return HAL_ERROR;  /* No message available */

  uint32_t ff  = fcm_read(&can->FF);
  uint32_t id0 = fcm_read(&can->ID0);
  uint32_t id1 = fcm_read(&can->ID1);

  pHeader->DLC = ff & 0x0FU;
  pHeader->RTR = (ff & CAN_FF_RTR) ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  pHeader->Timestamp = 0;
  pHeader->FilterMatchIndex = 0;

  if (ff & CAN_FF_FF)
  {
    /* Extended frame */
    uint32_t d0 = fcm_read(&can->DATA0);
    uint32_t d1 = fcm_read(&can->DATA1);
    uint32_t ext_id = (id0 << 24) | (id1 << 16) | (d0 << 8) | d1;
    pHeader->ExtId = ext_id >> 3;
    pHeader->StdId = 0;
    pHeader->IDE = CAN_ID_EXT;

    if (pHeader->RTR == CAN_RTR_DATA)
    {
      aData[0] = (uint8_t)fcm_read(&can->DATA2);
      aData[1] = (uint8_t)fcm_read(&can->DATA3);
      aData[2] = (uint8_t)fcm_read(&can->DATA4);
      aData[3] = (uint8_t)fcm_read(&can->DATA5);
      aData[4] = (uint8_t)fcm_read(&can->DATA6);
      aData[5] = (uint8_t)fcm_read(&can->DATA7);
      aData[6] = (uint8_t)fcm_read(&can->DATA8);
      aData[7] = (uint8_t)fcm_read(&can->DATA9);
    }
  }
  else
  {
    /* Standard frame */
    uint32_t std_id = ((id0 << 8) | id1) >> 5;
    pHeader->StdId = std_id & 0x7FFU;
    pHeader->ExtId = 0;
    pHeader->IDE = CAN_ID_STD;

    if (pHeader->RTR == CAN_RTR_DATA)
    {
      aData[0] = (uint8_t)fcm_read(&can->DATA0);
      aData[1] = (uint8_t)fcm_read(&can->DATA1);
      aData[2] = (uint8_t)fcm_read(&can->DATA2);
      aData[3] = (uint8_t)fcm_read(&can->DATA3);
      aData[4] = (uint8_t)fcm_read(&can->DATA4);
      aData[5] = (uint8_t)fcm_read(&can->DATA5);
      aData[6] = (uint8_t)fcm_read(&can->DATA6);
      aData[7] = (uint8_t)fcm_read(&can->DATA7);
    }
  }

  /* Release receive buffer */
  can->CMR = CAN_CMR_RRB;
  return HAL_OK;
}

/* ── Notifications ─────────────────────────────────────────────────────────── */

HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan,
                                               uint32_t ActiveITs)
{
  /* Collapse all requested ITs into the FCM IER bits */
  uint32_t ier = fcm_read(&hcan->Instance->IER);
  ier |= (ActiveITs & 0xFFU);
  hcan->Instance->IER = ier;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_DeactivateNotification(CAN_HandleTypeDef *hcan,
                                                 uint32_t InactiveITs)
{
  uint32_t ier = fcm_read(&hcan->Instance->IER);
  ier &= ~(InactiveITs & 0xFFU);
  hcan->Instance->IER = ier;
  return HAL_OK;
}

/* ── IRQ handler ───────────────────────────────────────────────────────────── */

void HAL_CAN_IRQHandler(CAN_HandleTypeDef *hcan)
{
  CAN_TypeDef *can = hcan->Instance;

  /* Reading IR clears all interrupts except RI */
  uint32_t ir = fcm_read(&can->IR);
  uint32_t sr = fcm_read(&can->SR);

  /* Receive interrupt */
  if (ir & CAN_IR_RI)
  {
    HAL_CAN_RxFifo0MsgPendingCallback(hcan);
  }

  /* Transmit complete interrupt */
  if (ir & CAN_IR_TI)
  {
    HAL_CAN_TxMailbox0CompleteCallback(hcan);
  }

  /* Error warning interrupt (also signals bus-off entry/exit on FCM) */
  if (ir & CAN_IR_EI)
  {
    if (sr & CAN_SR_BS)
    {
      hcan->ErrorCode |= HAL_CAN_ERROR_BOF;
    }
    if (sr & CAN_SR_ES)
    {
      hcan->ErrorCode |= HAL_CAN_ERROR_EWG;
    }
    HAL_CAN_ErrorCallback(hcan);
  }

  /* Data overrun interrupt */
  if (ir & CAN_IR_DOI)
  {
    hcan->ErrorCode |= HAL_CAN_ERROR_RX_FOV0;
    can->CMR = CAN_CMR_CDO;  /* Clear data overrun */
    HAL_CAN_ErrorCallback(hcan);
  }

  /* Error passive interrupt */
  if (ir & CAN_IR_EPI)
  {
    hcan->ErrorCode |= HAL_CAN_ERROR_EPV;
    HAL_CAN_ErrorCallback(hcan);
  }

  /* Arbitration lost interrupt */
  if (ir & CAN_IR_ALI)
  {
    hcan->ErrorCode |= HAL_CAN_ERROR_TX_ALST0;
    HAL_CAN_ErrorCallback(hcan);
  }

  /* Bus error interrupt */
  if (ir & CAN_IR_BEI)
  {
    uint32_t ecc = fcm_read(&can->ECC);
    /* ECC[7:5] = error type: 0=bit, 1=form, 2=stuff, 3=other */
    switch ((ecc >> 5) & 0x07U)
    {
      case 0: hcan->ErrorCode |= HAL_CAN_ERROR_BD;  break;
      case 1: hcan->ErrorCode |= HAL_CAN_ERROR_FOR; break;
      case 2: hcan->ErrorCode |= HAL_CAN_ERROR_STF; break;
      default: hcan->ErrorCode |= HAL_CAN_ERROR_CRC; break;
    }
    HAL_CAN_ErrorCallback(hcan);
  }

  /* Wake-up interrupt */
  if (ir & CAN_IR_WUI)
  {
    HAL_CAN_WakeUpFromRxMsgCallback(hcan);
  }
}

/* ── Weak callbacks ────────────────────────────────────────────────────────── */

__weak void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) { UNUSED(hcan); }
__weak void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) { UNUSED(hcan); }
__weak void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) { UNUSED(hcan); }
__weak void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan)   { UNUSED(hcan); }
__weak void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan)   { UNUSED(hcan); }
__weak void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan)   { UNUSED(hcan); }
__weak void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) { UNUSED(hcan); }
__weak void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan)      { UNUSED(hcan); }
__weak void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan) { UNUSED(hcan); }
__weak void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan)      { UNUSED(hcan); }
__weak void HAL_CAN_SleepCallback(CAN_HandleTypeDef *hcan)            { UNUSED(hcan); }
__weak void HAL_CAN_WakeUpFromRxMsgCallback(CAN_HandleTypeDef *hcan)  { UNUSED(hcan); }
__weak void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)            { UNUSED(hcan); }

/* ── State / Error ─────────────────────────────────────────────────────────── */

HAL_CAN_StateTypeDef HAL_CAN_GetState(const CAN_HandleTypeDef *hcan)
{
  uint32_t sr = fcm_read(&hcan->Instance->SR);
  if (sr & CAN_SR_BS)
    return HAL_CAN_STATE_ERROR;
  return hcan->State;
}

uint32_t HAL_CAN_GetError(const CAN_HandleTypeDef *hcan)
{
  return hcan->ErrorCode;
}

HAL_StatusTypeDef HAL_CAN_ResetError(CAN_HandleTypeDef *hcan)
{
  hcan->ErrorCode = HAL_CAN_ERROR_NONE;
  return HAL_OK;
}

/* ── Sleep / Wake ──────────────────────────────────────────────────────────── */

HAL_StatusTypeDef HAL_CAN_RequestSleep(CAN_HandleTypeDef *hcan)
{
  CAN_TypeDef *can = hcan->Instance;
  uint32_t mod = fcm_read(&can->MOD);
  if (mod & CAN_MOD_RM) return HAL_ERROR;  /* Must be in operating mode */
  can->MOD = mod | CAN_MOD_SM;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_WakeUp(CAN_HandleTypeDef *hcan)
{
  CAN_TypeDef *can = hcan->Instance;
  uint32_t mod = fcm_read(&can->MOD);
  can->MOD = mod & ~CAN_MOD_SM;
  return HAL_OK;
}

uint32_t HAL_CAN_IsSleepActive(const CAN_HandleTypeDef *hcan)
{
  uint32_t mod = fcm_read(&hcan->Instance->MOD);
  return (mod & CAN_MOD_SM) ? 1U : 0U;
}

#endif /* HAL_CAN_MODULE_ENABLED */
