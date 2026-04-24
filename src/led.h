#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef enum {
  LED_MODE_OFF,
  LED_MODE_BREATHE,
  LED_MODE_BLINK,
} led_mode_t;

void led_init(void);
void led_set_mode(led_mode_t mode);
void led_update(void);

#endif /* LED_H */
