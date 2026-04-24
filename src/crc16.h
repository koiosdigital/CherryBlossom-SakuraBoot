#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/* CRC16-CCITT (Katapult protocol) */
uint16_t crc16_ccitt(const uint8_t *data, uint32_t len);

#endif /* CRC16_H */
