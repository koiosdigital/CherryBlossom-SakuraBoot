#include "led.h"
#include "main.h"

static led_mode_t current_mode = LED_MODE_OFF;
static uint32_t last_tick = 0;
static uint8_t pwm_phase = 0;

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

void led_set_mode(led_mode_t mode) {
  current_mode = mode;
  last_tick = HAL_GetTick();
  pwm_phase = 0;
}

void led_update(void) {
  uint32_t now = HAL_GetTick();

  switch (current_mode) {
  case LED_MODE_BREATHE: {
    /* Triangle wave PWM, ~2s period, 16ms update */
    if ((now - last_tick) < 16)
      return;
    last_tick = now;

    /* 128-step triangle wave over ~2048ms */
    pwm_phase++;
    uint8_t brightness = (pwm_phase & 0x40) ? (127 - pwm_phase) : pwm_phase;
    brightness &= 0x3F; /* 0-63 */

    /* Simple on/off PWM within the 16ms tick */
    static uint8_t sub_phase = 0;
    sub_phase = (sub_phase + 1) & 0x0F;
    uint8_t threshold = brightness >> 2; /* 0-15 */
    HAL_GPIO_WritePin(LED_HBT_GPIO_Port, LED_HBT_Pin,
                      (sub_phase < threshold) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    break;
  }

  case LED_MODE_BLINK:
    /* Fast blink: 50ms on, 50ms off */
    if ((now - last_tick) >= 50) {
      last_tick = now;
      HAL_GPIO_TogglePin(LED_HBT_GPIO_Port, LED_HBT_Pin);
    }
    break;

  case LED_MODE_OFF:
  default:
    HAL_GPIO_WritePin(LED_HBT_GPIO_Port, LED_HBT_Pin, GPIO_PIN_RESET);
    break;
  }
}
