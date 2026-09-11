#ifndef DEV_FS_DEVICES_H
#define DEV_FS_DEVICES_H

#include "src/file_system/ramfs/ramfs.h"
#include "src/drivers/esp32_spi/spi.h"
#include "arch/xtensa_lx6/xtensa.h"

void dev_fs_register_hw_devices(void);
void uart0_hw_putchar(char c);
int  uart0_hw_read(void *buf, uint32_t len);

#endif /* DEV_FS_DEVICES_H */
