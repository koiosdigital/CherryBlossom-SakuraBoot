#include "main.h"
#include "can_boot.h"
#include "flash.h"
#include "led.h"
#include "can_hw.h"

#include <string.h>

/* Boot magic — shared with application, stored at top of RAM */
#define REQUEST_BOOTLOADER  0x5984E3FA6CA1589BULL
#define BOOT_MAGIC_ADDR     ((volatile uint64_t *)((uint32_t)&_estack - 8))

/* Application vector table address (after bootloader + metadata page) */
#define APP_VECTOR_ADDR     0x08002400U
#define APP_FLASH_END       0x0800FC00U  /* NVS starts here */
#define RAM_START           0x20000000U
#define RAM_END             0x20005000U

extern uint32_t _estack;

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
  led_set_mode(LED_MODE_BREATHE);

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
  uint32_t *vectors = (uint32_t *)APP_VECTOR_ADDR;
  uint32_t sp = vectors[0];
  uint32_t pc = vectors[1];

  if (sp < RAM_START || sp > RAM_END)
    return false;
  if (pc < APP_VECTOR_ADDR || pc > APP_FLASH_END)
    return false;
  if (sp == 0xFFFFFFFFU || pc == 0xFFFFFFFFU)
    return false;
  return true;
}

static void jump_to_app(void) {
  uint32_t *vectors = (uint32_t *)APP_VECTOR_ADDR;

  /* Deinit HAL if it was initialized (it wasn't in fast path, but be safe) */
  __disable_irq();

  SCB->VTOR = APP_VECTOR_ADDR;
  __set_MSP(vectors[0]);

  void (*app_reset)(void) = (void (*)(void))vectors[1];
  app_reset();

  /* Should never reach here */
  while (1) {}
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
