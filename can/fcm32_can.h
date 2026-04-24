/**
 * FCM32 CAN driver — adapted from STM32F0xx StdPeriph Library v1.5.0
 * for use with FCM32 F103-compatible clone CAN IP (SJA1000-like).
 *
 * Original: stm32f0xx_can.h by MCD Application Team
 * Changes:  - Replaced stm32f0xx.h include with fcm32_can_regs.h
 *           - Renamed CAN_TypeDef references to FCM32_CAN_TypeDef
 *           - Removed RCC dependency (caller manages clocks)
 */

#ifndef FCM32_CAN_H
#define FCM32_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fcm32_can_regs.h"

/* Suppress assert_param if not defined by HAL */
#ifndef assert_param
#define assert_param(expr) ((void)0U)
#endif

/*
 * FunctionalState, FlagStatus, ITStatus are already defined by
 * stm32f1xx.h when building with HAL. We rely on those definitions.
 * Include stm32f1xx_hal.h to get them (also provides __IO via CMSIS).
 */
#include "stm32f1xx_hal.h"

/* ── Initialization structures ──────────────────────────────────────── */

typedef struct {
  uint8_t CAN_Prescaler;   /* Baud rate prescaler (0-63) */
  uint8_t CAN_Mode;        /* Operating mode (ignored for FCM) */
  FlagStatus CAN_SAM;      /* Sample point: RESET=1 sample, SET=3 samples */
  uint8_t CAN_EWLR;        /* Error Warning Limit Register */
  uint8_t CAN_SJW;         /* Synchronization Jump Width (0-3) */
  uint8_t CAN_BS1;         /* Bit Segment 1 (0-15) */
  uint8_t CAN_BS2;         /* Bit Segment 2 (0-7) */
  FunctionalState CAN_TTCM; /* Ignored for FCM */
  FunctionalState CAN_ABOM; /* Ignored for FCM */
  FunctionalState CAN_AWUM; /* Ignored for FCM */
  FunctionalState CAN_NART; /* Ignored for FCM */
  FunctionalState CAN_RFLM; /* Ignored for FCM */
  FunctionalState CAN_TXFP; /* Ignored for FCM */
} FCM32_CAN_InitTypeDef;

typedef struct {
  uint8_t CAN_FilterId0;
  uint8_t CAN_FilterId1;
  uint8_t CAN_FilterId2;
  uint8_t CAN_FilterId3;
  uint8_t CAN_FilterMaskId0;
  uint8_t CAN_FilterMaskId1;
  uint8_t CAN_FilterMaskId2;
  uint8_t CAN_FilterMaskId3;
} FCM32_CAN_FilterInitTypeDef;

/* ── Message structures ─────────────────────────────────────────────── */

typedef struct {
  uint32_t StdId;    /* Standard identifier (0-0x7FF) */
  uint32_t ExtId;    /* Extended identifier (0-0x1FFFFFFF) */
  uint8_t  IDE;      /* CAN_Id_Standard or CAN_Id_Extended */
  uint8_t  RTR;      /* CAN_RTR_Data or CAN_RTR_Remote */
  uint8_t  DLC;      /* Data length code (0-8) */
  uint8_t  SS;       /* Single-shot transmission (FCM only) */
  uint8_t  Data[8];
} FCM32_CanTxMsg;

typedef struct {
  uint32_t StdId;
  uint32_t ExtId;
  uint8_t  IDE;
  uint8_t  RTR;
  uint8_t  DLC;
  uint8_t  Data[8];
  uint8_t  FMI;      /* Unused for FCM */
} FCM32_CanRxMsg;

/* ── Constants ──────────────────────────────────────────────────────── */

#define CAN_InitStatus_Failed    ((uint8_t)0x00)
#define CAN_InitStatus_Success   ((uint8_t)0x01)

#define CAN_Id_Standard          ((uint32_t)0x00000000)
#define CAN_Id_Extended          ((uint32_t)0x00000080)

#define CAN_RTR_Data             ((uint32_t)0x00000000)
#define CAN_RTR_Remote           ((uint32_t)0x00000040)

#define CAN_TxStatus_Failed      ((uint8_t)0x00)
#define CAN_TxStatus_Ok          ((uint8_t)0x01)
#define CAN_TxStatus_Pending     ((uint8_t)0x02)
#define CAN_TxStatus_BufLocked   ((uint8_t)0x04)

/* Operating modes (bits in MOD register) */
#define CAN_OperatingMode_Normal       ((uint32_t)0x00)
#define CAN_OperatingMode_Reset        ((uint32_t)0x01)
#define CAN_OperatingMode_LisenOnly    ((uint32_t)0x02)
#define CAN_OperatingMode_SelfTest     ((uint32_t)0x04)
#define CAN_OperatingMode_SingleFilter ((uint32_t)0x08)
#define CAN_OperatingMode_Sleep        ((uint32_t)0x10)

#define CAN_ModeStatus_Failed    ((uint8_t)0x00)
#define CAN_ModeStatus_Success   ((uint8_t)0x01)

/* Status register flags */
#define CAN_STATUS_RBS  ((uint32_t)0x01)
#define CAN_STATUS_DOS  ((uint32_t)0x02)
#define CAN_STATUS_TBS  ((uint32_t)0x04)
#define CAN_STATUS_TCS  ((uint32_t)0x08)
#define CAN_STATUS_RS   ((uint32_t)0x10)
#define CAN_STATUS_TS   ((uint32_t)0x20)
#define CAN_STATUS_ES   ((uint32_t)0x40)
#define CAN_STATUS_BS   ((uint32_t)0x80)

/* Interrupt flags */
#define CAN_IT_RI   ((uint32_t)0x01)
#define CAN_IT_TI   ((uint32_t)0x02)
#define CAN_IT_EI   ((uint32_t)0x04)
#define CAN_IT_DOI  ((uint32_t)0x08)
#define CAN_IT_WUI  ((uint32_t)0x10)
#define CAN_IT_EPI  ((uint32_t)0x20)
#define CAN_IT_ALI  ((uint32_t)0x40)
#define CAN_IT_BEI  ((uint32_t)0x80)

/* Error codes from ECC register */
#define CAN_ErrorCode_NoErr           ((uint8_t)0x00)
#define CAN_ErrorCode_StuffErr        ((uint8_t)0x10)
#define CAN_ErrorCode_FormErr         ((uint8_t)0x20)
#define CAN_ErrorCode_ACKErr          ((uint8_t)0x30)
#define CAN_ErrorCode_BitRecessiveErr ((uint8_t)0x40)
#define CAN_ErrorCode_BitDominantErr  ((uint8_t)0x50)
#define CAN_ErrorCode_CRCErr          ((uint8_t)0x60)
#define CAN_ErrorCode_SoftwareSetErr  ((uint8_t)0x70)

/* ── API Functions ──────────────────────────────────────────────────── */

/* Init / Deinit */
uint8_t FCM32_CAN_Init(FCM32_CAN_TypeDef *CANx, FCM32_CAN_InitTypeDef *CAN_InitStruct);
void    FCM32_CAN_FilterInit(FCM32_CAN_TypeDef *CANx, FCM32_CAN_FilterInitTypeDef *CAN_FilterInitStruct);
void    FCM32_CAN_StructInit(FCM32_CAN_InitTypeDef *CAN_InitStruct);
void    FCM32_CAN_AutoCfg_Baud(FCM32_CAN_InitTypeDef *CAN_InitStruct, uint32_t SrcClk, uint32_t baud);

/* TX / RX */
uint8_t FCM32_CAN_Transmit(FCM32_CAN_TypeDef *CANx, FCM32_CanTxMsg *TxMessage);
uint8_t FCM32_CAN_TransmitStatus(FCM32_CAN_TypeDef *CANx);
void    FCM32_CAN_CancelTransmit(FCM32_CAN_TypeDef *CANx);
void    FCM32_CAN_Receive(FCM32_CAN_TypeDef *CANx, FCM32_CanRxMsg *RxMessage);
void    FCM32_CAN_FIFORelease(FCM32_CAN_TypeDef *CANx);
uint8_t FCM32_CAN_MessagePending(FCM32_CAN_TypeDef *CANx);

/* Operating modes */
uint8_t FCM32_CAN_OperatingModeRequest(FCM32_CAN_TypeDef *CANx, uint32_t CAN_OperatingMode, FunctionalState NewState);

/* Error management */
uint8_t FCM32_CAN_GetLastErrorCode(FCM32_CAN_TypeDef *CANx);
uint8_t FCM32_CAN_GetReceiveErrorCounter(FCM32_CAN_TypeDef *CANx);
uint8_t FCM32_CAN_GetTransmitErrorCounter(FCM32_CAN_TypeDef *CANx);

/* Interrupts */
void       FCM32_CAN_ITConfig(FCM32_CAN_TypeDef *CANx, uint32_t CAN_IT, FunctionalState NewState);
FlagStatus FCM32_CAN_GetFlagStatus(FCM32_CAN_TypeDef *CANx, uint32_t CAN_FLAG);
ITStatus   FCM32_CAN_GetITStatus(FCM32_CAN_TypeDef *CANx, uint32_t CAN_IT);
void       FCM32_CAN_ClearITPendingBit(FCM32_CAN_TypeDef *CANx);
void       FCM32_CAN_ClearDataOverrun(FCM32_CAN_TypeDef *CANx);

#ifdef __cplusplus
}
#endif

#endif /* FCM32_CAN_H */
