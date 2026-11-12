/* The keyboards Arges can drive, and the packets it sends them. No Windows code, so test_effects.c builds anywhere.
 * To add a keyboard, add a row to KEYBOARDS. If it needs different packets, change the razer_ functions here. */
#include <string.h>

typedef struct {
    unsigned short vid, pid;
    unsigned char iface;
    unsigned char txn;
    unsigned char cls;
    unsigned char set_cmd;
    unsigned char show_cmd;
} Keyboard;

static const Keyboard KEYBOARDS[] = {
    {0x1532, 0x0294, 2, 0x1F, 0x0F, 0x03, 0x02}, /* Razer Ornata V3 X */
    {0x1532, 0x02A2, 2, 0x1F, 0x0F, 0x03, 0x02}, /* Razer Ornata V3 X, later revision */
};
#define KEYBOARD_COUNT ((int)(sizeof KEYBOARDS / sizeof *KEYBOARDS))

static inline void razer_packet(unsigned char p[91], const Keyboard *kb, unsigned char cmd, unsigned char size,
                         const unsigned char *args, int nargs)
{
    memset(p, 0, 91);
    p[2] = kb->txn;
    p[6] = size;
    p[7] = kb->cls;
    p[8] = cmd;
    memcpy(p + 9, args, nargs);
    for (int i = 3; i < 89; i++)
        p[89] ^= p[i];
}

static inline void razer_set_colour(unsigned char p[91], const Keyboard *kb, const unsigned char rgb[3])
{
    unsigned char args[8] = {0, 0, 0, 0, 0, rgb[0], rgb[1], rgb[2]};
    razer_packet(p, kb, kb->set_cmd, 8, args, 8);
}

static inline void razer_show(unsigned char p[91], const Keyboard *kb)
{
    static const unsigned char args[3] = {0, 0, 0x08};
    razer_packet(p, kb, kb->show_cmd, 0x0C, args, 3);
}
