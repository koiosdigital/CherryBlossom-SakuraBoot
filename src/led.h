#ifndef LED_H
#define LED_H

#include <stdint.h>

void led_init(void);
void led_update(void);

/* Brief activity flash (call on each processed command) */
void led_activity(void);

#endif /* LED_H */
