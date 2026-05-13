#ifndef CAN_HELPERS_H
#define CAN_HELPERS_H

#include <stdint.h>

/* Derive the 6-byte device UUID from the STM32 96-bit unique device ID
 * via fasthash64(seed=0xA16231A7). Byte-identical with the same helper
 * in sakura_firmware so the host can correlate a module's bootloader-
 * mode entry and its app-mode entry by UUID. */
void can_get_device_id(uint8_t *device_id);

#endif /* CAN_HELPERS_H */
