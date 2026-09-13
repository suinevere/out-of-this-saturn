#ifndef CDDA_WANT_H
#define CDDA_WANT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    CDDA_WANT_KEEP  = 0,
    CDDA_WANT_CLEAR = 1,
    CDDA_WANT_STOP  = 2,
    CDDA_WANT_PLAY  = 3
} cdda_want_action;

cdda_want_action cdda_want(int wanted, int last_wanted, int playing);

#ifdef __cplusplus
}
#endif

#endif
