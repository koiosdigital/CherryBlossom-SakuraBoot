#include "can_helpers.h"

#include <string.h>

#include "fasthash.h"
#include "stm32f1xx_hal.h"

void can_get_device_id(uint8_t *device_id) {
  uint64_t hash = fasthash64((void *)UID_BASE, 12, 0xA16231A7);
  memcpy(device_id, &hash, 6);
}
