#ifndef FCM32_CAN_REGS_H
#define FCM32_CAN_REGS_H

#include <stdint.h>

/* FCM32 CAN peripheral register structure (SJA1000-compatible) */
typedef struct {
  volatile uint32_t MOD;       /* Mode,                     offset: 0x00 */
  volatile uint32_t CMR;       /* Command,                  offset: 0x04 */
  volatile uint32_t SR;        /* Status,                   offset: 0x08 */
  volatile uint32_t IR;        /* Interrupt (read-to-clear), offset: 0x0C */
  volatile uint32_t IER;       /* Interrupt enable,         offset: 0x10 */
  uint32_t          RESERVED0; /* Reserved,                 offset: 0x14 */
  volatile uint32_t BTR0;      /* Bus Timing 0,             offset: 0x18 */
  volatile uint32_t BTR1;      /* Bus Timing 1,             offset: 0x1C */
  uint32_t          RESERVED1; /* Reserved,                 offset: 0x20 */
  uint32_t          RESERVED2; /* Reserved,                 offset: 0x24 */
  uint32_t          RESERVED3; /* Reserved,                 offset: 0x28 */
  volatile uint32_t ALC;       /* Arbitration Lost Capture, offset: 0x2C */
  volatile uint32_t ECC;       /* Error Code Capture,       offset: 0x30 */
  volatile uint32_t EWLR;      /* Error Warning Limit,      offset: 0x34 */
  volatile uint32_t RXERR;     /* Receive Error Counter,    offset: 0x38 */
  volatile uint32_t TXERR;     /* Transmit Error Counter,   offset: 0x3C */
  volatile uint32_t FF;        /* Frame Format / Filter,    offset: 0x40 */
  volatile uint32_t ID0;       /* ID byte 0,                offset: 0x44 */
  volatile uint32_t ID1;       /* ID byte 1,                offset: 0x48 */
  volatile uint32_t DATA0;     /* Data byte 0,              offset: 0x4C */
  volatile uint32_t DATA1;     /* Data byte 1,              offset: 0x50 */
  volatile uint32_t DATA2;     /* Data byte 2,              offset: 0x54 */
  volatile uint32_t DATA3;     /* Data byte 3,              offset: 0x58 */
  volatile uint32_t DATA4;     /* Data byte 4,              offset: 0x5C */
  volatile uint32_t DATA5;     /* Data byte 5,              offset: 0x60 */
  volatile uint32_t DATA6;     /* Data byte 6,              offset: 0x64 */
  volatile uint32_t DATA7;     /* Data byte 7,              offset: 0x68 */
  volatile uint32_t DATA8;     /* Data byte 8 (ext only),   offset: 0x6C */
  volatile uint32_t DATA9;     /* Data byte 9 (ext only),   offset: 0x70 */
  volatile uint32_t RMC;       /* Receive Message Count,    offset: 0x74 */
  volatile uint32_t RBSA;      /* RX Buffer Start Address,  offset: 0x78 */
  volatile uint32_t CDR;       /* Clock Divider,            offset: 0x7C */
} FCM32_CAN_TypeDef;

/* CAN peripheral base address — same as CAN1 on STM32F103 clone */
#define FCM32_CAN_BASE    (0x40000000UL + 0x00006400UL)
#define FCM32_CAN         ((FCM32_CAN_TypeDef *)FCM32_CAN_BASE)

/* --- MOD register bits --- */
#define CAN_MOD_RM        ((uint32_t)0x00000001)  /* Reset Mode */
#define CAN_MOD_LOM       ((uint32_t)0x00000002)  /* Listen Only Mode */
#define CAN_MOD_STM       ((uint32_t)0x00000004)  /* Self Test Mode */
#define CAN_MOD_AFM       ((uint32_t)0x00000008)  /* Acceptance Filter Mode */
#define CAN_MOD_SM        ((uint32_t)0x00000010)  /* Sleep Mode */

/* --- CMR register bits --- */
#define CAN_CMR_TR        ((uint32_t)0x00000001)  /* Transmission Request */
#define CAN_CMR_AT        ((uint32_t)0x00000002)  /* Abort Transmission */
#define CAN_CMR_RRB       ((uint32_t)0x00000004)  /* Release Receive Buffer */
#define CAN_CMR_CDO       ((uint32_t)0x00000008)  /* Clear Data Overrun */
#define CAN_CMR_SRR       ((uint32_t)0x00000010)  /* Self Reception Request */

/* --- SR register bits --- */
#define CAN_SR_RBS        ((uint32_t)0x00000001)  /* Receive Buffer Status */
#define CAN_SR_DOS        ((uint32_t)0x00000002)  /* Data Overrun Status */
#define CAN_SR_TBS        ((uint32_t)0x00000004)  /* Transmit Buffer Status */
#define CAN_SR_TCS        ((uint32_t)0x00000008)  /* Transmission Complete */
#define CAN_SR_RS         ((uint32_t)0x00000010)  /* Receive Status */
#define CAN_SR_TS         ((uint32_t)0x00000020)  /* Transmit Status */
#define CAN_SR_ES         ((uint32_t)0x00000040)  /* Error Status */
#define CAN_SR_BS         ((uint32_t)0x00000080)  /* Bus Off Status */

/* --- IR register bits (read-to-clear) --- */
#define CAN_IR_RI         ((uint32_t)0x00000001)  /* Receive Interrupt */
#define CAN_IR_TI         ((uint32_t)0x00000002)  /* Transmit Interrupt */
#define CAN_IR_EI         ((uint32_t)0x00000004)  /* Error Warning Interrupt */
#define CAN_IR_DOI        ((uint32_t)0x00000008)  /* Data Overrun Interrupt */
#define CAN_IR_WUI        ((uint32_t)0x00000010)  /* Wake-Up Interrupt */
#define CAN_IR_EPI        ((uint32_t)0x00000020)  /* Error Passive Interrupt */
#define CAN_IR_ALI        ((uint32_t)0x00000040)  /* Arbitration Lost Interrupt */
#define CAN_IR_BEI        ((uint32_t)0x00000080)  /* Bus Error Interrupt */

/* --- IER register bits --- */
#define CAN_IER_RIE       ((uint32_t)0x00000001)  /* Receive Interrupt Enable */
#define CAN_IER_TIE       ((uint32_t)0x00000002)  /* Transmit Interrupt Enable */
#define CAN_IER_EIE       ((uint32_t)0x00000004)  /* Error Warning Interrupt Enable */
#define CAN_IER_DOIE      ((uint32_t)0x00000008)  /* Data Overrun Interrupt Enable */
#define CAN_IER_WUIE      ((uint32_t)0x00000010)  /* Wake-Up Interrupt Enable */
#define CAN_IER_EPIE      ((uint32_t)0x00000020)  /* Error Passive Interrupt Enable */
#define CAN_IER_ALIE      ((uint32_t)0x00000040)  /* Arbitration Lost Interrupt Enable */
#define CAN_IER_BEIE      ((uint32_t)0x00000080)  /* Bus Error Interrupt Enable */

#endif /* FCM32_CAN_REGS_H */
