#include "flash.h"
#include "stm32f1xx.h"
#include <string.h>

static void flash_unlock(void) {
  if (FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = FLASH_KEY1_Msk;
    FLASH->KEYR = FLASH_KEY2_Msk;
  }
}

static void flash_lock(void) {
  FLASH->CR |= FLASH_CR_LOCK;
}

static void flash_wait(void) {
  while (FLASH->SR & FLASH_SR_BSY) {}
}

static int flash_erase_page(uint32_t page_addr) {
  flash_wait();
  FLASH->CR |= FLASH_CR_PER;
  FLASH->AR = page_addr;
  FLASH->CR |= FLASH_CR_STRT;
  flash_wait();
  FLASH->CR &= ~FLASH_CR_PER;

  if (FLASH->SR & (FLASH_SR_WRPRTERR | FLASH_SR_PGERR)) {
    FLASH->SR = FLASH_SR_WRPRTERR | FLASH_SR_PGERR;
    return -1;
  }
  return 0;
}

int flash_write_block(uint32_t addr, const void *data) {
  /* Bounds check */
  if (addr < FLASH_WRITE_START || (addr + FLASH_BLOCK_SIZE) > FLASH_WRITE_END)
    return -1;

  /* Alignment check */
  if (addr & (FLASH_BLOCK_SIZE - 1))
    return -2;

  flash_unlock();

  /* Erase page if this block starts on a page boundary */
  uint32_t page_addr = addr & ~(BOOT_FLASH_PAGE_SIZE - 1);
  if (addr == page_addr) {
    if (flash_erase_page(page_addr) != 0) {
      flash_lock();
      return -3;
    }
  }

  /* Program half-words (STM32F1 flash programming unit = 16 bits) */
  const uint16_t *src = (const uint16_t *)data;
  volatile uint16_t *dst = (volatile uint16_t *)addr;

  for (uint32_t i = 0; i < FLASH_BLOCK_SIZE / 2; i++) {
    flash_wait();
    FLASH->CR |= FLASH_CR_PG;
    dst[i] = src[i];
    flash_wait();
    FLASH->CR &= ~FLASH_CR_PG;

    if (FLASH->SR & (FLASH_SR_WRPRTERR | FLASH_SR_PGERR)) {
      FLASH->SR = FLASH_SR_WRPRTERR | FLASH_SR_PGERR;
      flash_lock();
      return -4;
    }
  }

  flash_lock();

  /* Verify */
  if (memcmp(data, (const void *)addr, FLASH_BLOCK_SIZE) != 0)
    return -5;

  return 0;
}
