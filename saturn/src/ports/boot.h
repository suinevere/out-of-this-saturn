#ifndef BOOT_H
#define BOOT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CHAINBOOT_ENABLED
#define CHAINBOOT_ENABLED 0
#endif

void boot_bios(void);
int chainboot_available(void);
void chainboot_run(void);

#ifdef __cplusplus
}
#endif

#endif
