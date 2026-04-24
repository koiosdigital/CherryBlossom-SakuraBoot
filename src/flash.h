#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

/* Writable region: metadata page + app area (excludes bootloader and NVS) */
#define FLASH_WRITE_START  0x08002000U
#define FLASH_WRITE_END    0x0800FC00U
#define BOOT_FLASH_PAGE_SIZE  1024U
#define FLASH_BLOCK_SIZE   64U

/* Write a 64-byte block to flash. Erases page if block is at page boundary.
 * Returns 0 on success, negative on error. */
int flash_write_block(uint32_t addr, const void *data);

#endif /* FLASH_H */
