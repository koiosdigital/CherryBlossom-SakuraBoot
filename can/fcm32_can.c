/**
 * FCM32 CAN driver — adapted from STM32F0xx StdPeriph Library v1.5.0.
 *
 * Original: stm32f0xx_can.c by MCD Application Team
 * Changes:  - Removed RCC dependency (caller manages clocks/reset)
 *           - Renamed types/functions to FCM32_ prefix
 *           - Removed unused CAN_DeInit, CAN_Sleep, CAN_WakeUp
 *           - Stripped debug read_all_can_addr
 */

#include "fcm32_can.h"

/* ── Init / Config ──────────────────────────────────────────────────── */

uint8_t FCM32_CAN_Init(FCM32_CAN_TypeDef* CANx,
                       FCM32_CAN_InitTypeDef* CAN_InitStruct) {
  uint32_t mod_buf;
  mod_buf = CANx->MOD;
  mod_buf = CANx->MOD;

  /* Ensure reset mode is entered */
  mod_buf |= CAN_MOD_RM;
  CANx->MOD = mod_buf;

  /* Bus timing registers */
  CANx->BTR0 = ((uint32_t)CAN_InitStruct->CAN_SJW << 6) |
               ((uint32_t)CAN_InitStruct->CAN_Prescaler);
  CANx->BTR1 = ((uint32_t)CAN_InitStruct->CAN_SAM << 7) |
               ((uint32_t)CAN_InitStruct->CAN_BS2 << 4) |
               ((uint32_t)CAN_InitStruct->CAN_BS1);

  /* Error warning limit */
  CANx->EWLR = CAN_InitStruct->CAN_EWLR;

  return CAN_InitStatus_Success;
}

void FCM32_CAN_FilterInit(FCM32_CAN_TypeDef* CANx,
                          FCM32_CAN_FilterInitTypeDef* f) {
  CANx->FF = f->CAN_FilterId0;
  CANx->ID0 = f->CAN_FilterId1;
  CANx->ID1 = f->CAN_FilterId2;
  CANx->DATA0 = f->CAN_FilterId3;

  CANx->DATA1 = f->CAN_FilterMaskId0;
  CANx->DATA2 = f->CAN_FilterMaskId1;
  CANx->DATA3 = f->CAN_FilterMaskId2;
  CANx->DATA4 = f->CAN_FilterMaskId3;
}

void FCM32_CAN_StructInit(FCM32_CAN_InitTypeDef* s) {
  s->CAN_Prescaler = 0;
  s->CAN_SJW = 0;
  s->CAN_BS1 = 0;
  s->CAN_BS2 = 0;
  s->CAN_SAM = RESET;
  s->CAN_EWLR = 0x96;
}

void FCM32_CAN_AutoCfg_Baud(FCM32_CAN_InitTypeDef* s, uint32_t SrcClk,
                            uint32_t baud) {
  unsigned int i, value = baud, record = 1;
  unsigned int remain = 0, sumPrescaler = 0;

  if (baud == 0 || SrcClk == 0) return;

  sumPrescaler = SrcClk / baud;
  sumPrescaler = sumPrescaler / 2;

  for (i = 25; i > 3; i--) {
    remain = sumPrescaler - ((sumPrescaler / i) * i);
    if (remain == 0) {
      record = i;
      break;
    } else {
      if (remain < value) {
        value = remain;
        record = i;
      }
    }
  }

  s->CAN_SJW = 0;
  s->CAN_Prescaler = (sumPrescaler / record) - 1;
  s->CAN_BS2 = (record - 3) / 3;
  s->CAN_BS1 = (record - 3) - s->CAN_BS2;
}

/* ── TX / RX ────────────────────────────────────────────────────────── */

uint8_t FCM32_CAN_Transmit(FCM32_CAN_TypeDef* CANx, FCM32_CanTxMsg* TxMessage) {
  uint32_t mod_buf, sr_buf;
  uint32_t StdId_buf, ExtId_buf;

  mod_buf = CANx->MOD;
  mod_buf = CANx->MOD;
  sr_buf = CANx->SR;
  sr_buf = CANx->SR;

  StdId_buf = TxMessage->StdId << 5;
  ExtId_buf = TxMessage->ExtId << 3;

  if (!(sr_buf & CAN_SR_TBS)) return CAN_TxStatus_BufLocked;

  CANx->FF = TxMessage->IDE | TxMessage->RTR | TxMessage->DLC;

  if (TxMessage->IDE != CAN_Id_Extended) {
    CANx->ID0 = (uint8_t)(StdId_buf >> 8);
    CANx->ID1 = (uint8_t)(StdId_buf);
    if (TxMessage->RTR != CAN_RTR_Remote) {
      CANx->DATA0 = TxMessage->Data[0];
      CANx->DATA1 = TxMessage->Data[1];
      CANx->DATA2 = TxMessage->Data[2];
      CANx->DATA3 = TxMessage->Data[3];
      CANx->DATA4 = TxMessage->Data[4];
      CANx->DATA5 = TxMessage->Data[5];
      CANx->DATA6 = TxMessage->Data[6];
      CANx->DATA7 = TxMessage->Data[7];
    }
  } else {
    CANx->ID0 = (uint8_t)(ExtId_buf >> 24);
    CANx->ID1 = (uint8_t)(ExtId_buf >> 16);
    CANx->DATA0 = (uint8_t)(ExtId_buf >> 8);
    CANx->DATA1 = (uint8_t)(ExtId_buf);
    if (TxMessage->RTR != CAN_RTR_Remote) {
      CANx->DATA2 = TxMessage->Data[0];
      CANx->DATA3 = TxMessage->Data[1];
      CANx->DATA4 = TxMessage->Data[2];
      CANx->DATA5 = TxMessage->Data[3];
      CANx->DATA6 = TxMessage->Data[4];
      CANx->DATA7 = TxMessage->Data[5];
      CANx->DATA8 = TxMessage->Data[6];
      CANx->DATA9 = TxMessage->Data[7];
    }
  }

  if (mod_buf & CAN_MOD_STM) {
    CANx->CMR = CAN_CMR_SRR | CAN_CMR_AT;
  } else if (TxMessage->SS) {
    CANx->CMR = CAN_CMR_TR | CAN_CMR_AT;
  } else {
    CANx->CMR = CAN_CMR_TR;
  }

  return CAN_TxStatus_Pending;
}

uint8_t FCM32_CAN_TransmitStatus(FCM32_CAN_TypeDef* CANx) {
  uint32_t sr_buf;
  sr_buf = CANx->SR;
  sr_buf = CANx->SR;
  return (sr_buf & CAN_SR_TCS) ? CAN_TxStatus_Ok : CAN_TxStatus_Failed;
}

void FCM32_CAN_CancelTransmit(FCM32_CAN_TypeDef* CANx) {
  CANx->CMR = CAN_CMR_AT;
}

void FCM32_CAN_Receive(FCM32_CAN_TypeDef* CANx, FCM32_CanRxMsg* RxMessage) {
  uint32_t ff_buf, id0_buf, id1_buf;
  uint32_t data_buf[10];
  uint16_t std_buf;
  uint32_t ext_buf;

  ff_buf = CANx->FF;
  ff_buf = CANx->FF;
  id0_buf = CANx->ID0;
  id0_buf = CANx->ID0;
  id1_buf = CANx->ID1;
  id1_buf = CANx->ID1;

  /* Double-read pattern for reliable volatile access */
  data_buf[0] = CANx->DATA0;
  data_buf[0] = CANx->DATA0;
  data_buf[1] = CANx->DATA1;
  data_buf[1] = CANx->DATA1;
  data_buf[2] = CANx->DATA2;
  data_buf[2] = CANx->DATA2;
  data_buf[3] = CANx->DATA3;
  data_buf[3] = CANx->DATA3;
  data_buf[4] = CANx->DATA4;
  data_buf[4] = CANx->DATA4;
  data_buf[5] = CANx->DATA5;
  data_buf[5] = CANx->DATA5;
  data_buf[6] = CANx->DATA6;
  data_buf[6] = CANx->DATA6;
  data_buf[7] = CANx->DATA7;
  data_buf[7] = CANx->DATA7;
  data_buf[8] = CANx->DATA8;
  data_buf[8] = CANx->DATA8;
  data_buf[9] = CANx->DATA9;
  data_buf[9] = CANx->DATA9;

  RxMessage->IDE = ff_buf & CAN_Id_Extended;
  RxMessage->RTR = ff_buf & CAN_RTR_Remote;
  RxMessage->DLC = ff_buf & 0x0F;

  if (RxMessage->IDE != CAN_Id_Extended) {
    std_buf = id1_buf + (id0_buf << 8);
    RxMessage->StdId = std_buf >> 5;
    for (int i = 0; i < 8; i++) RxMessage->Data[i] = (uint8_t)data_buf[i];
  } else {
    ext_buf =
        data_buf[1] + (data_buf[0] << 8) + (id1_buf << 16) + (id0_buf << 24);
    RxMessage->ExtId = ext_buf >> 3;
    for (int i = 0; i < 8; i++) RxMessage->Data[i] = (uint8_t)data_buf[i + 2];
  }

  /* Release receive buffer */
  CANx->CMR = CAN_CMR_RRB;
}

void FCM32_CAN_FIFORelease(FCM32_CAN_TypeDef* CANx) { CANx->CMR = CAN_CMR_RRB; }

uint8_t FCM32_CAN_MessagePending(FCM32_CAN_TypeDef* CANx) {
  uint32_t rmc;
  rmc = CANx->RMC;
  rmc = CANx->RMC;
  return (uint8_t)rmc;
}

/* ── Operating modes ────────────────────────────────────────────────── */

uint8_t FCM32_CAN_OperatingModeRequest(FCM32_CAN_TypeDef* CANx,
                                       uint32_t CAN_OperatingMode,
                                       FunctionalState NewState) {
  uint32_t mod_buf;
  mod_buf = CANx->MOD;
  mod_buf = CANx->MOD;

  if (NewState != DISABLE)
    mod_buf |= CAN_OperatingMode;
  else
    mod_buf &= ~CAN_OperatingMode;

  CANx->MOD = mod_buf;
  return CAN_ModeStatus_Success;
}

/* ── Error management ───────────────────────────────────────────────── */

uint8_t FCM32_CAN_GetLastErrorCode(FCM32_CAN_TypeDef* CANx) {
  uint32_t ecc;
  ecc = CANx->ECC;
  ecc = CANx->ECC;
  return (uint8_t)ecc;
}

uint8_t FCM32_CAN_GetReceiveErrorCounter(FCM32_CAN_TypeDef* CANx) {
  uint32_t rxerr;
  rxerr = CANx->RXERR;
  rxerr = CANx->RXERR;
  return (uint8_t)rxerr;
}

uint8_t FCM32_CAN_GetTransmitErrorCounter(FCM32_CAN_TypeDef* CANx) {
  uint32_t txerr;
  txerr = CANx->TXERR;
  txerr = CANx->TXERR;
  return (uint8_t)txerr;
}

/* ── Interrupts ─────────────────────────────────────────────────────── */

void FCM32_CAN_ITConfig(FCM32_CAN_TypeDef* CANx, uint32_t CAN_IT,
                        FunctionalState NewState) {
  uint32_t ier;
  ier = CANx->IER;
  ier = CANx->IER;

  if (NewState != DISABLE)
    ier |= CAN_IT;
  else
    ier &= ~CAN_IT;

  CANx->IER = ier;
}

FlagStatus FCM32_CAN_GetFlagStatus(FCM32_CAN_TypeDef* CANx, uint32_t CAN_FLAG) {
  uint32_t sr;
  sr = CANx->SR;
  sr = CANx->SR;
  return (sr & CAN_FLAG) ? SET : RESET;
}

ITStatus FCM32_CAN_GetITStatus(FCM32_CAN_TypeDef* CANx, uint32_t CAN_IT) {
  uint32_t ir;
  ir = CANx->IR;
  ir = CANx->IR;
  return (ir & CAN_IT) ? SET : RESET;
}

void FCM32_CAN_ClearITPendingBit(FCM32_CAN_TypeDef* CANx) {
  volatile uint32_t ir;
  ir = CANx->IR;
  ir = CANx->IR; /* Reading IR clears all except RI */
  (void)ir;
}

void FCM32_CAN_ClearDataOverrun(FCM32_CAN_TypeDef* CANx) {
  CANx->CMR = CAN_CMR_CDO;
}
