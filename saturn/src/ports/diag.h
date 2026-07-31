#ifndef DIAG_H
#define DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

void diag_row(int row, const char *fmt, int a, int b, int c, int d, int e);

#define BOOT_DIAG 0

#if BOOT_DIAG
#define BOOT_SAY(step) diag_row(22, "boot: step %d", (step), 0, 0, 0, 0)
#else
#define BOOT_SAY(step) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
