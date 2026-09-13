#ifndef CDDA_GATE_H
#define CDDA_GATE_H

#ifdef __cplusplus
extern "C" {
#endif

int cdda_gate_enter(void);

int cdda_gate_exit(void);

int cdda_gate_depth(void);

void cdda_gate_reset(void);

#ifdef __cplusplus
}
#endif

#endif
