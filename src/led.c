#include "led.h"
#include "main.h"

#define BLINK_HALF_PERIOD  250  /* 2Hz = 500ms period = 250ms half */
#define ACT_FLASH_MS        30  /* brief on-pulse per command */

static uint32_t blink_tick = 0;
static uint32_t act_off_tick = 0;
static uint8_t  act_active = 0;

void led_init(void) {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = LED_HBT_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_HBT_GPIO_Port, &gpio);
  HAL_GPIO_WritePin(LED_HBT_GPIO_Port, LED_HBT_Pin, GPIO_PIN_RESET);
}

void led_activity(void) {
  act_active = 1;
  act_off_tick = HAL_GetTick() + ACT_FLASH_MS;
  HAL_GPIO_WritePin(LED_HBT_GPIO_Port, LED_HBT_Pin, GPIO_PIN_SET);
}

void led_update(void) {
  uint32_t now = HAL_GetTick();

  /* Activity flash takes priority */
  if (act_active) {
    if (now >= act_off_tick) {
      act_active = 0;
      HAL_GPIO_WritePin(LED_HBT_GPIO_Port, LED_HBT_Pin, GPIO_PIN_RESET);
    }
    return;
  }

  /* Idle: 2Hz blink */
  if ((now - blink_tick) >= BLINK_HALF_PERIOD) {
    blink_tick = now;
    HAL_GPIO_TogglePin(LED_HBT_GPIO_Port, LED_HBT_Pin);
  }
}
