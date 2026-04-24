/**
  ******************************************************************************
  * @file    stm32f1xx_hal_can.h
  * @brief   HAL CAN driver for FCM32 PeliCAN-compatible CAN peripheral.
  *
  *          This replaces the STM32 bxCAN HAL driver. The FCM32 CAN IP block
  *          is SJA1000/PeliCAN-compatible with a single TX buffer, single RX
  *          FIFO, and simple 4-byte acceptance filter. The HAL API surface is
  *          kept compatible so application code needs minimal changes.
  ******************************************************************************
  */

#ifndef STM32F1xx_HAL_CAN_H
#define STM32F1xx_HAL_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal_def.h"

#if defined(CAN1)

/* ── Types ─────────────────────────────────────────────────────────────────── */

typedef enum
{
  HAL_CAN_STATE_RESET     = 0x00U,
  HAL_CAN_STATE_READY     = 0x01U,
  HAL_CAN_STATE_LISTENING = 0x02U,
  HAL_CAN_STATE_ERROR     = 0x05U
} HAL_CAN_StateTypeDef;

/**
  * @brief  CAN init structure — FCM32 PeliCAN bit-timing parameters.
  *         Register values are raw (0-indexed).
  */
typedef struct
{
  uint32_t Prescaler;          /*!< BRP register value (actual prescaler = Prescaler+1).
                                    Range 0..63. CAN clk is PCLK1/(2*(Prescaler+1)). */
  uint32_t Mode;               /*!< @ref CAN_operating_mode */
  uint32_t SyncJumpWidth;      /*!< SJW register value (0..3 = 1..4 TQ) */
  uint32_t TimeSeg1;           /*!< TSEG1 register value (0..15 = 1..16 TQ) */
  uint32_t TimeSeg2;           /*!< TSEG2 register value (0..7 = 1..8 TQ) */
  FunctionalState AutoBusOff;  /*!< ENABLE sets EWLR=0xFF for auto bus-off recovery */
} CAN_InitTypeDef;

/**
  * @brief  CAN acceptance filter — written to the filter window in reset mode.
  *         FCM uses double-filter mode for standard frames: two independent
  *         11-bit ID + mask pairs packed into 4 ID bytes + 4 mask bytes.
  *         Only FilterBank 0 is supported; FilterActivation is honoured.
  */
typedef struct
{
  uint32_t FilterIdHigh;           /*!< Std ID for filter pair 1 (0..0x7FF) */
  uint32_t FilterIdLow;           /*!< Unused (compat) */
  uint32_t FilterMaskIdHigh;      /*!< Mask for filter pair 1 (0..0x7FF, 1=must match) */
  uint32_t FilterMaskIdLow;       /*!< Unused (compat) */
  uint32_t FilterFIFOAssignment;  /*!< Ignored (single FIFO) — kept for API compat */
  uint32_t FilterBank;            /*!< 0 or 1 — only bank 0 writes HW, bank 1 is SW-only */
  uint32_t FilterMode;            /*!< Ignored (always ID+mask) */
  uint32_t FilterScale;           /*!< Ignored (always 32-bit packing) */
  uint32_t FilterActivation;      /*!< ENABLE / DISABLE */
  uint32_t SlaveStartFilterBank;  /*!< Ignored */
} CAN_FilterTypeDef;

typedef struct
{
  uint32_t StdId;
  uint32_t ExtId;
  uint32_t IDE;
  uint32_t RTR;
  uint32_t DLC;
  FunctionalState TransmitGlobalTime;  /*!< Ignored — kept for API compat */
} CAN_TxHeaderTypeDef;

typedef struct
{
  uint32_t StdId;
  uint32_t ExtId;
  uint32_t IDE;
  uint32_t RTR;
  uint32_t DLC;
  uint32_t Timestamp;         /*!< Always 0 — no HW timestamp */
  uint32_t FilterMatchIndex;  /*!< Always 0 */
} CAN_RxHeaderTypeDef;

typedef struct
{
  CAN_TypeDef              *Instance;
  CAN_InitTypeDef           Init;
  __IO HAL_CAN_StateTypeDef State;
  __IO uint32_t             ErrorCode;
} CAN_HandleTypeDef;

/* ── Error codes ───────────────────────────────────────────────────────────── */

#define HAL_CAN_ERROR_NONE            (0x00000000U)
#define HAL_CAN_ERROR_EWG             (0x00000001U)
#define HAL_CAN_ERROR_EPV             (0x00000002U)
#define HAL_CAN_ERROR_BOF             (0x00000004U)
#define HAL_CAN_ERROR_STF             (0x00000008U)
#define HAL_CAN_ERROR_FOR             (0x00000010U)
#define HAL_CAN_ERROR_ACK             (0x00000020U)
#define HAL_CAN_ERROR_BR              (0x00000040U)
#define HAL_CAN_ERROR_BD              (0x00000080U)
#define HAL_CAN_ERROR_CRC             (0x00000100U)
#define HAL_CAN_ERROR_RX_FOV0         (0x00000200U)
#define HAL_CAN_ERROR_RX_FOV1         (0x00000400U)
#define HAL_CAN_ERROR_TX_ALST0        (0x00000800U)
#define HAL_CAN_ERROR_TX_TERR0        (0x00001000U)
#define HAL_CAN_ERROR_TX_ALST1        (0x00002000U)
#define HAL_CAN_ERROR_TX_TERR1        (0x00004000U)
#define HAL_CAN_ERROR_TX_ALST2        (0x00008000U)
#define HAL_CAN_ERROR_TX_TERR2        (0x00010000U)
#define HAL_CAN_ERROR_TIMEOUT         (0x00020000U)
#define HAL_CAN_ERROR_NOT_INITIALIZED (0x00040000U)
#define HAL_CAN_ERROR_NOT_READY       (0x00080000U)
#define HAL_CAN_ERROR_NOT_STARTED     (0x00100000U)
#define HAL_CAN_ERROR_PARAM           (0x00200000U)
#define HAL_CAN_ERROR_INTERNAL        (0x00800000U)

/* ── Operating mode ────────────────────────────────────────────────────────── */

#define CAN_MODE_NORMAL             (0x00000000U)
#define CAN_MODE_LOOPBACK           (0x00000004U)  /* Self-test mode */
#define CAN_MODE_SILENT             (0x00000002U)  /* Listen-only mode */
#define CAN_MODE_SILENT_LOOPBACK    (0x00000006U)

/* ── Bit-timing helpers (raw register values) ──────────────────────────────── */

#define CAN_SJW_1TQ   (0U)
#define CAN_SJW_2TQ   (1U)
#define CAN_SJW_3TQ   (2U)
#define CAN_SJW_4TQ   (3U)

#define CAN_BS1_1TQ   (0U)
#define CAN_BS1_2TQ   (1U)
#define CAN_BS1_3TQ   (2U)
#define CAN_BS1_4TQ   (3U)
#define CAN_BS1_5TQ   (4U)
#define CAN_BS1_6TQ   (5U)
#define CAN_BS1_7TQ   (6U)
#define CAN_BS1_8TQ   (7U)
#define CAN_BS1_9TQ   (8U)
#define CAN_BS1_10TQ  (9U)
#define CAN_BS1_11TQ  (10U)
#define CAN_BS1_12TQ  (11U)
#define CAN_BS1_13TQ  (12U)
#define CAN_BS1_14TQ  (13U)
#define CAN_BS1_15TQ  (14U)
#define CAN_BS1_16TQ  (15U)

#define CAN_BS2_1TQ   (0U)
#define CAN_BS2_2TQ   (1U)
#define CAN_BS2_3TQ   (2U)
#define CAN_BS2_4TQ   (3U)
#define CAN_BS2_5TQ   (4U)
#define CAN_BS2_6TQ   (5U)
#define CAN_BS2_7TQ   (6U)
#define CAN_BS2_8TQ   (7U)

/* ── Filter constants (API compat) ─────────────────────────────────────────── */

#define CAN_FILTERMODE_IDMASK   (0U)
#define CAN_FILTERMODE_IDLIST   (1U)
#define CAN_FILTERSCALE_16BIT   (0U)
#define CAN_FILTERSCALE_32BIT   (1U)
#define CAN_FILTER_DISABLE      (0U)
#define CAN_FILTER_ENABLE       (1U)
#define CAN_FILTER_FIFO0        (0U)
#define CAN_FILTER_FIFO1        (1U)

/* ── Identifier type ───────────────────────────────────────────────────────── */

#define CAN_ID_STD              (0x00000000U)
#define CAN_ID_EXT              (0x00000080U)  /* matches FCM FF bit 7 */

/* ── RTR ───────────────────────────────────────────────────────────────────── */

#define CAN_RTR_DATA            (0x00000000U)
#define CAN_RTR_REMOTE          (0x00000040U)  /* matches FCM FF bit 6 */

/* ── RX FIFO (single on FCM, kept for API compat) ─────────────────────────── */

#define CAN_RX_FIFO0            (0U)
#define CAN_RX_FIFO1            (1U)

/* ── TX Mailbox (single on FCM, kept for API compat) ──────────────────────── */

#define CAN_TX_MAILBOX0         (0x00000001U)
#define CAN_TX_MAILBOX1         (0x00000002U)
#define CAN_TX_MAILBOX2         (0x00000004U)

/* ── Interrupt flags — mapped to FCM IER bits ─────────────────────────────── */

#define CAN_IT_TX_MAILBOX_EMPTY      CAN_IER_TIE
#define CAN_IT_RX_FIFO0_MSG_PENDING  CAN_IER_RIE
#define CAN_IT_RX_FIFO0_FULL         (0U)       /* no HW equivalent */
#define CAN_IT_RX_FIFO0_OVERRUN      CAN_IER_DOIE
#define CAN_IT_RX_FIFO1_MSG_PENDING  (0U)       /* single FIFO */
#define CAN_IT_RX_FIFO1_FULL         (0U)
#define CAN_IT_RX_FIFO1_OVERRUN      (0U)
#define CAN_IT_WAKEUP                CAN_IER_WUIE
#define CAN_IT_SLEEP_ACK             (0U)
#define CAN_IT_ERROR_WARNING         CAN_IER_EIE
#define CAN_IT_ERROR_PASSIVE         CAN_IER_EPIE
#define CAN_IT_BUSOFF                CAN_IER_EIE   /* bus-off fires EI on FCM */
#define CAN_IT_LAST_ERROR_CODE       CAN_IER_BEIE
#define CAN_IT_ERROR                 CAN_IER_BEIE

/* ── Status flag helpers (kept for __HAL_CAN_CLEAR_FLAG compat) ────────────── */

#define CAN_FLAG_ERRI               (0x00000102U)

/* ── Macros ────────────────────────────────────────────────────────────────── */

#define __HAL_CAN_RESET_HANDLE_STATE(__HANDLE__) \
  ((__HANDLE__)->State = HAL_CAN_STATE_RESET)

#define __HAL_CAN_ENABLE_IT(__HANDLE__, __IT__) \
  do { uint32_t _ier = (__HANDLE__)->Instance->IER; \
       _ier = (__HANDLE__)->Instance->IER; \
       (__HANDLE__)->Instance->IER = _ier | (__IT__); } while(0)

#define __HAL_CAN_DISABLE_IT(__HANDLE__, __IT__) \
  do { uint32_t _ier = (__HANDLE__)->Instance->IER; \
       _ier = (__HANDLE__)->Instance->IER; \
       (__HANDLE__)->Instance->IER = _ier & ~(__IT__); } while(0)

#define __HAL_CAN_GET_IT_SOURCE(__HANDLE__, __IT__) \
  ((__HANDLE__)->Instance->IER & (__IT__))

/* On FCM, flags are read-only or read-to-clear; this is a no-op */
#define __HAL_CAN_GET_FLAG(__HANDLE__, __FLAG__)  (0U)
#define __HAL_CAN_CLEAR_FLAG(__HANDLE__, __FLAG__) ((void)0)

/* ── Function prototypes ───────────────────────────────────────────────────── */

HAL_StatusTypeDef HAL_CAN_Init(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_DeInit(CAN_HandleTypeDef *hcan);
void              HAL_CAN_MspInit(CAN_HandleTypeDef *hcan);
void              HAL_CAN_MspDeInit(CAN_HandleTypeDef *hcan);

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan,
                                       const CAN_FilterTypeDef *sFilterConfig);

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_Stop(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan,
                                       const CAN_TxHeaderTypeDef *pHeader,
                                       const uint8_t aData[],
                                       uint32_t *pTxMailbox);
HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *hcan,
                                         uint32_t TxMailboxes);
uint32_t          HAL_CAN_GetTxMailboxesFreeLevel(const CAN_HandleTypeDef *hcan);
uint32_t          HAL_CAN_IsTxMessagePending(const CAN_HandleTypeDef *hcan,
                                             uint32_t TxMailboxes);
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *hcan,
                                       uint32_t RxFifo,
                                       CAN_RxHeaderTypeDef *pHeader,
                                       uint8_t aData[]);
uint32_t          HAL_CAN_GetRxFifoFillLevel(const CAN_HandleTypeDef *hcan,
                                             uint32_t RxFifo);

HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan,
                                               uint32_t ActiveITs);
HAL_StatusTypeDef HAL_CAN_DeactivateNotification(CAN_HandleTypeDef *hcan,
                                                 uint32_t InactiveITs);
void              HAL_CAN_IRQHandler(CAN_HandleTypeDef *hcan);

/* Callbacks (weak, override in application) */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_SleepCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_WakeUpFromRxMsgCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan);

HAL_CAN_StateTypeDef HAL_CAN_GetState(const CAN_HandleTypeDef *hcan);
uint32_t             HAL_CAN_GetError(const CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef    HAL_CAN_ResetError(CAN_HandleTypeDef *hcan);

/* Not in bxCAN HAL but useful for FCM */
HAL_StatusTypeDef HAL_CAN_RequestSleep(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_WakeUp(CAN_HandleTypeDef *hcan);
uint32_t          HAL_CAN_IsSleepActive(const CAN_HandleTypeDef *hcan);
uint32_t          HAL_CAN_GetTxTimestamp(const CAN_HandleTypeDef *hcan,
                                         uint32_t TxMailbox);

#endif /* CAN1 */

#ifdef __cplusplus
}
#endif

#endif /* STM32F1xx_HAL_CAN_H */
