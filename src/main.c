#include "main.h"
#include "can_boot.h"
#include "fasthash.h"
#include "flash.h"
#include "led.h"
#include "can_hw.h"

#include <stddef.h>
#include <string.h>

/* Boot magic — shared with application via .boot_magic linker section.
 * Placed at start of RAM (0x20000000), not touched by startup code. */
#define REQUEST_BOOTLOADER  0x5984E3FA6CA1589BULL
extern uint64_t _boot_magic;
#define BOOT_MAGIC_ADDR     ((volatile uint64_t *)&_boot_magic)

/* Flash layout */
#define META_ADDR           0x08002000U
#define APP_VECTOR_ADDR     0x08002400U  /* app starts after 1KB metadata page */
#define APP_FLASH_END       0x0800FC00U  /* NVS starts here */
#define RAM_START           0x20000000U
#define RAM_END             0x20005000U
#define APP_MAX_SIZE        (APP_FLASH_END - APP_VECTOR_ADDR)

/* Metadata layout — must match scripts/gen_metadata.py byte-for-byte. */
typedef struct __attribute__((packed)) {
  char     magic[8];          /* "SAKURA\0\0" */
  uint32_t app_size;
  uint64_t app_hash;
  uint32_t app_version;
  char     app_variant_name[32];
  uint8_t  reserved[8];
  uint32_t meta_crc;
} sakura_metadata_t;

static const char META_MAGIC[8] = {'S', 'A', 'K', 'U', 'R', 'A', 0, 0};

static void SystemClock_Config(void);
static bool app_is_valid(void);
static void jump_to_app(void) __attribute__((noreturn));

int main(void) {
  /* Check boot magic before any peripheral init */
  bool stay = false;
  if (*BOOT_MAGIC_ADDR == REQUEST_BOOTLOADER) {
    *BOOT_MAGIC_ADDR = 0;
    stay = true;
  }

  if (!stay && app_is_valid()) {
    jump_to_app();
  }

  /* Stay in bootloader — init everything */
  HAL_Init();
  SystemClock_Config();
  led_init();

  /* CAN at 50kbps, filter only admin ID 0x3F0 */
  can_hw_init(50000);
  can_hw_set_filter(0x3F0, 0x000);
  can_hw_start();

  can_boot_init();

  while (1) {
    /* Poll CAN RX */
    while (can_hw_rx_pending() > 0) {
      CAN_HW_Message msg;
      if (can_hw_receive(&msg)) {
        can_boot_process_rx(msg.data, msg.dlc);
      }
    }

    /* Try to send pending TX frames */
    can_boot_tx_poll();

    /* Update LED */
    led_update();
  }
}

static bool app_is_valid(void) {
  const sakura_metadata_t *meta = (const sakura_metadata_t *)META_ADDR;

  if (memcmp(meta->magic, META_MAGIC, sizeof(META_MAGIC)) != 0)
    return false;

  if (meta->app_size == 0 || meta->app_size > APP_MAX_SIZE)
    return false;

  uint32_t crc = fasthash32(meta, offsetof(sakura_metadata_t, meta_crc), 0);
  if (crc != meta->meta_crc)
    return false;

  uint64_t hash = fasthash64((const void *)APP_VECTOR_ADDR, meta->app_size, 0);
  if (hash != meta->app_hash)
    return false;

  uint32_t *vectors = (uint32_t *)APP_VECTOR_ADDR;
  uint32_t sp = vectors[0];
  uint32_t pc = vectors[1];
  if (sp < RAM_START || sp > RAM_END)
    return false;
  if (pc < APP_VECTOR_ADDR || pc > APP_FLASH_END)
    return false;

  return true;
}

static void jump_to_app(void) {
  uint32_t *vectors = (uint32_t *)APP_VECTOR_ADDR;
  uint32_t sp = vectors[0];
  uint32_t pc = vectors[1];
  SCB->VTOR = APP_VECTOR_ADDR;
  __DSB();
  __ISB();
  asm volatile("MSR msp, %0\n  bx %1" : : "r"(sp), "r"(pc) : "memory");
  __builtin_unreachable();
}

static void SystemClock_Config(void) {
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  /* HSE 16MHz / 2 = 8MHz, PLL x9 = 72MHz */
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.HSEPredivValue = RCC_HSE_PREDIV_DIV2;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    Error_Handler();
  }

  /* HCLK=72MHz, APB1=36MHz (CAN), APB2=72MHz */
  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV2;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {}
}

/* SysTick handler — required by HAL for HAL_GetTick() */
void SysTick_Handler(void) {
  HAL_IncTick();
}

/* Stubs required by newlib init */
void _init(void) {}
void _fini(void) {}
