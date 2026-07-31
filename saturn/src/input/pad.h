#ifndef PAD_H
#define PAD_H

#define PAD_BIT_UP     (1u << 0)
#define PAD_BIT_DOWN   (1u << 1)
#define PAD_BIT_LEFT   (1u << 2)
#define PAD_BIT_RIGHT  (1u << 3)
#define PAD_BIT_PAUSE  (1u << 5)

#define PAD_BIT_A      (1u << 6)
#define PAD_BIT_B      (1u << 7)
#define PAD_BIT_C      (1u << 8)
#define PAD_BIT_L      (1u << 9)
#define PAD_BIT_R      (1u << 10)
#define PAD_BIT_X      (1u << 11)
#define PAD_BIT_Y      (1u << 12)
#define PAD_BIT_Z      (1u << 13)

#define PAD_RESET_CHORD (PAD_BIT_A | PAD_BIT_B | PAD_BIT_C | PAD_BIT_PAUSE)

#endif
