/* No Windows code, so test_effects.c builds anywhere. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

enum { MAX_COLOURS = 16 };

typedef struct {
    double in_ms, out_ms, curve, echo;
    int echoes;
} Flash;

static inline double flash_length(const Flash *f)
{
    return (f->in_ms + f->out_ms) * (1 + f->echoes);
}

static inline double flash_level(const Flash *f, double t, double from)
{
    double pulse = f->in_ms + f->out_ms, peak = 1;
    if (t >= flash_length(f)) return 0;
    int n = (int)(t / pulse);
    t -= n * pulse;
    if (n) from = 0, peak = f->echo;
    if (t < f->in_ms) return from + (peak - from) * t / f->in_ms;
    return peak * pow(1 - (t - f->in_ms) / f->out_ms, f->curve);
}

/* in the settings window's order */
enum { STATIC_OFF, STILL, BREATHE, HEARTBEAT, EMBER, BEACON, SPARKS, RAINBOW, AURORA, STATIC_EFFECTS };
static const wchar_t *const STATIC_NAMES[STATIC_EFFECTS] = {
    L"off", L"still", L"breathe", L"heartbeat", L"ember", L"beacon", L"sparks", L"rainbow", L"aurora"};
/* 0 = the effect doesn't use it */
static const int STATIC_PERIOD[STATIC_EFFECTS] = {0, 0, 5000, 1200, 1000, 3000, 1500, 20000, 20000};
static const int STATIC_STRENGTH[STATIC_EFFECTS] = {0, 0, 70, 60, 35, 80, 60, 0, 50};

/* in the settings window's order */
enum { REACTIVE_OFF, REACTIVE_FLASH, REACTIVE_RAINBOW, TYPING_SPEED, REACTIVE_HEARTBEAT, REACTIVE_EFFECTS };
static const wchar_t *const REACTIVE_NAMES[REACTIVE_EFFECTS] = {L"off", L"flash", L"rainbow", L"speed", L"heartbeat"};
enum { HEART_DURATIONS = 5, HEART_LINGER = 2000 };

static inline int name_index(const wchar_t *s, const wchar_t *const *names, int n)
{
    for (int i = 0; i < n; i++) {
        const wchar_t *a = s, *b = names[i];
        while (*a && towlower(*a) == *b) a++, b++;
        if (!*a && !*b) return i;
    }
    return -1;
}

static inline int reactive_index(const wchar_t *s)
{
    int i = name_index(s, REACTIVE_NAMES, REACTIVE_EFFECTS);
    return i >= 0 ? i : REACTIVE_FLASH;
}

typedef struct { int effect, speed, strength; } Static;

static inline double static_period(const Static *s)
{
    return STATIC_PERIOD[s->effect] * pow(2, (5 - s->speed) / 2.0);
}

static inline int static_of(int effect, int speed, int strength, Static *s)
{
    if (effect < 0) return 0;
    s->effect = effect;
    s->speed = speed < 0 ? 5 : speed < 1 ? 1 : speed > 10 ? 10 : speed;
    s->strength = strength < 0 ? STATIC_STRENGTH[effect] : strength > 100 ? 100 : strength;
    return 1;
}

static inline double breath_pulse(double t_ms, double period_ms)
{
    return (1 + cos(6.283185307179586 * t_ms / period_ms)) / 2;
}

static inline double ember_pulse(double last, double random, double weight)
{
    return last * (1 - weight) + (1 - random) * weight;
}

static inline double beacon_pulse(double t_ms, double period_ms)
{
    return exp(-fmod(t_ms, period_ms) / period_ms * 20);
}

static inline double heartbeat_pulse(double t_ms, double period_ms)
{
    double x = fmod(t_ms, period_ms) / period_ms, beat = exp(-x * 25);
    if (x >= 0.2 && 0.6 * exp(-(x - 0.2) * 25) > beat) beat = 0.6 * exp(-(x - 0.2) * 25);
    return beat;
}

static inline double aurora_pos(double t_ms, double period_ms, double strength)
{
    double a = 6.283185307179586 * t_ms / period_ms;
    double p = 0.5 * strength * (0.6 * sin(a) + 0.4 * sin(a / 0.37));
    return p - floor(p);
}

static inline double spark_level(double since_ms)
{
    return since_ms >= 0 && since_ms < 250 ? 1 - since_ms / 250 : 0;
}

static inline double heat_after(double heat, double ms) { return heat * exp(-ms / 2000); }
static inline double heat_press(double heat) { return heat + 0.15 * (1 - heat); }

/* 0 at about one key a second, 1 at about eight */
static inline double typing_pace(double heat)
{
    double p = (heat - 0.31) / 0.43;
    return p < 0 ? 0 : p > 1 ? 1 : p;
}

static inline void blend(const int a[3], const int b[3], double t, int out[3])
{
    for (int i = 0; i < 3; i++) out[i] = (int)(a[i] + (b[i] - a[i]) * t + 0.5);
}

static inline void rotate_hue(const int in[3], double turns, int out[3])
{
    double r = in[0], g = in[1], b = in[2], v = fmax(r, fmax(g, b)), d = v - fmin(r, fmin(g, b));
    if (d <= 0) {
        memcpy(out, in, 3 * sizeof *in);
        return;
    }
    double h = v == r ? (g - b) / d : v == g ? 2 + (b - r) / d : 4 + (r - g) / d, s = d / v;
    h = fmod(h + turns * 6, 6);
    if (h < 0) h += 6;
    double f = h - floor(h), p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    double c[6][3] = {{v, t, p}, {q, v, p}, {p, v, t}, {p, q, v}, {t, p, v}, {v, p, q}};
    for (int i = 0; i < 3; i++) out[i] = (int)(c[(int)h % 6][i] + 0.5);
}

static inline void cycle_colour(const int list[][3], int n, double pos, int out[3])
{
    pos -= floor(pos);
    if (n < 2) {
        rotate_hue(list[0], pos, out);
        return;
    }
    double x = pos * n;
    int i = (int)x % n;
    blend(list[i], list[(i + 1) % n], x - floor(x), out);
}

static inline void ramp_colour(const int list[][3], int n, double t, int out[3])
{
    double x = (t < 0 ? 0 : t > 1 ? 1 : t) * (n - 1);
    int i = (int)x < n - 1 ? (int)x : n - 1;
    if (i == n - 1) memcpy(out, list[i], 3 * sizeof *out);
    else blend(list[i], list[i + 1], x - i, out);
}

static inline double pulse_colour(const int list[][3], int n, double pulse, double depth, int out[3])
{
    ramp_colour(list, n, 1 - pulse, out);
    return 1 - depth * (1 - pulse);
}

static inline void press_colour(int effect, const int cols[][3], int n, unsigned turn, double pace, int out[3])
{
    static const int white[3] = {255, 255, 255};
    memcpy(out, cols[turn % n], 3 * sizeof *out);
    if (effect == REACTIVE_RAINBOW) cycle_colour(cols, n, turn % 12 / 12.0, out);
    if (effect == TYPING_SPEED) blend(cols[0], n > 1 ? cols[n - 1] : white, n > 1 ? pace : pace * 0.7, out);
}

typedef struct { double ember, spark_at; int spark_colour; } StaticState; /* start with ember = 1 */

/* The light between key presses. base holds the colour to start from and ends as the effect's colour; returns how
 * bright it is, 0-1. now and dt are in ms: any steady clock, and the time since the last call. */
static inline double static_light(int effect, double period, double depth, const int list[][3], int n, double now,
                                  double dt, StaticState *s, int base[3])
{
    double bright = 1;
    switch (effect) {
    case BREATHE: bright = pulse_colour(list, n, breath_pulse(now, period), depth, base); break;
    case EMBER:
        s->ember = ember_pulse(s->ember, rand() / (double)RAND_MAX, fmin(0.5, 300 / period)); /* above 0.5 it strobes */
        bright = pulse_colour(list, n, s->ember, depth, base);
        break;
    case HEARTBEAT: bright = pulse_colour(list, n, heartbeat_pulse(now, period), depth, base); break;
    case BEACON: bright = pulse_colour(list, n, beacon_pulse(now, period), depth, base); break;
    case RAINBOW: cycle_colour(list, n, now / period, base); break;
    case STATIC_OFF: memset(base, 0, 3 * sizeof *base); break;
    case AURORA: cycle_colour(list, n, aurora_pos(now, period, depth), base); break;
    case SPARKS: {
        static const int white[3] = {255, 255, 255};
        if (rand() < RAND_MAX * fmin(1, dt / period)) s->spark_at = now, s->spark_colour = n > 1 ? 1 + rand() % (n - 1) : 0;
        blend(base, n > 1 ? list[s->spark_colour < n ? s->spark_colour : n - 1] : white, /* the list may have shrunk */
              spark_level(now - s->spark_at) * depth, base);
        break;
    }
    }
    return bright;
}

/* shortcut: northern hemisphere only; add a southern= setting if anyone needs it */
static inline int season_of(int month)
{
    return (month + 9) % 12 / 3;
}

static inline int parse_colours(const wchar_t *s, int c[][3], int max)
{
    for (int n = 0; n < max;) {
        int used = -1;
        swscanf(s, L" %d ,%d ,%d %n", &c[n][0], &c[n][1], &c[n][2], &used);
        if (used < 0) return 0;
        n++, s += used;
        if (!*s) return n;
        if (*s++ != L';') return 0;
    }
    return 0;
}

static inline int in_hours(int hour, int start, int end)
{
    return start > end ? (hour >= start || hour < end) : (hour >= start && hour < end);
}

enum { MAX_SLOTS = 4 };
typedef struct { int from, to; wchar_t target[64]; } Slot;
static inline int parse_schedule(const wchar_t *s, Slot slot[], int max)
{
    int n = 0;
    while (*s) {
        Slot r;
        int used = -1, len = 0;
        swscanf(s, L" %d - %d : %n", &r.from, &r.to, &used);
        if (used >= 0) s += used;
        while (s[len] && s[len] != L';') len++;
        while (len && iswspace(s[len - 1])) len--;
        if (used >= 0 && n < max && len > 0 && len < 64 && r.from >= 0 && r.from < 24 && r.to >= 0 && r.to < 24) {
            wcsncpy(r.target, s, len);
            r.target[len] = 0;
            slot[n++] = r;
        }
        while (*s && *s != L';') s++;
        if (*s) s++;
    }
    return n;
}

static inline int slot_at(const Slot slot[], int n, int hour)
{
    for (int i = 0; i < n; i++)
        if (in_hours(hour, slot[i].from, slot[i].to)) return i;
    return -1;
}

static inline void mix_colour(unsigned char out[3], const int flash[3], const int glow[][3], int glow_count,
                              const int still[3], double level, double bright)
{
    int steps = glow_count + 1, k = (int)((1 - level) * steps);
    if (k > glow_count) k = glow_count;
    const int *from = k ? glow[k - 1] : flash, *to = k < glow_count ? glow[k] : still;
    double t = (1 - level) * steps - k;
    for (int i = 0; i < 3; i++) out[i] = (unsigned char)((from[i] + (to[i] - from[i]) * t) * bright + 0.5);
}

/* The LEDs are linear and screens are sRGB, so a picked colour looks alike on both. */
static inline int led_to_screen(int v)
{
    double x = v / 255.0;
    return (int)(255 * (x <= 0.0031308 ? 12.92 * x : 1.055 * pow(x, 1 / 2.4) - 0.055) + 0.5);
}

static inline int screen_to_led(int v)
{
    double x = v / 255.0;
    return (int)(255 * (x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4)) + 0.5);
}

/* A profile's light settings, as load_lighting in arges.c reads them. */
typedef struct {
    Flash fx;
    Static st;
    int effect, hold, static_bright, reactive_bright, static_shuffle, reactive_shuffle;
    int cols[MAX_COLOURS][3], cols_n, glow[3][3], glow_n, still[MAX_COLOURS][3], still_n, seasons[4][3], seasons_n;
} Lighting;

/* What the light is doing now. Start with ss = {1}. Times are in ms on any steady clock. */
typedef struct {
    double last_press, last_tick, heat_time, heat, press_from, heart_ms;
    int held, flash_col[3];
    unsigned static_turn, reactive_turn;
    StaticState ss;
} LightState;

/* How bright the flash is now, 0-1; returns whether it is still running. */
static inline int light_flash(const Lighting *l, const LightState *s, double now, double *level)
{
    double t = now - s->last_press;
    if (s->held && t > l->fx.in_ms) t = l->fx.in_ms;
    int beat = l->effect == REACTIVE_HEARTBEAT;
    int running = l->effect != REACTIVE_OFF && t < (beat ? HEART_LINGER : flash_length(&l->fx));
    *level = !running ? 0 : beat ? heartbeat_pulse(s->heart_ms, HEART_DURATIONS * l->fx.out_ms) * (1 - t / HEART_LINGER)
                                 : flash_level(&l->fx, t, s->press_from);
    return running;
}

static inline void light_press(const Lighting *l, LightState *s, double now)
{
    s->heat = heat_press(heat_after(s->heat, now - s->heat_time));
    s->heat_time = now;
    /* shortcut: without knowing which key is down, another key during a hold looks like auto-repeat,
     * so neither starts a new flash */
    if (!s->held) {
        if (!light_flash(l, s, now, &s->press_from)) s->heart_ms = 0;
        s->static_turn += l->static_shuffle && l->still_n > 1 ? 1 + rand() % (l->still_n - 1) : 1;
        s->reactive_turn += l->reactive_shuffle && l->cols_n > 1 ? 1 + rand() % (l->cols_n - 1) : 1;
        s->last_press = now;
        s->held = l->hold;
    } else if (l->effect != TYPING_SPEED) return;
    if (l->cols_n) press_colour(l->effect, l->cols, l->cols_n, s->reactive_turn, typing_pace(s->heat), s->flash_col);
}

/* Returns whether anything changed. */
static inline int light_release(const Lighting *l, LightState *s, double now)
{
    if (!s->held) return 0;
    s->held = 0;
    if (now - s->last_press > l->fx.in_ms) s->last_press = now - l->fx.in_ms;
    return 1;
}

/* The colour to show now, scaled by bright; returns whether the flash is still running. month is 1-12. */
static inline int light_frame(const Lighting *l, LightState *s, double now, int month, double bright, unsigned char out[3])
{
    double dt = now - s->last_tick, level;
    s->last_tick = now;
    s->heat = heat_after(s->heat, now - s->heat_time);
    s->heat_time = now;
    s->heart_ms += fmin(dt, now - s->last_press) * (1 + 1.5 * typing_pace(s->heat));
    int running = light_flash(l, s, now, &level);

    const int (*list)[3] = l->seasons_n ? &l->seasons[season_of(month) * l->seasons_n / 4] : l->still;
    int n = l->seasons_n ? 1 : l->still_n, base[3], still[3], flash[3], glow[3][3];
    memcpy(base, list[l->st.effect == STILL ? s->static_turn % n : 0], sizeof base);
    double pulse = static_light(l->st.effect, static_period(&l->st), l->st.strength / 100.0, list, n, now, dt, &s->ss, base);
    for (int i = 0; i < 3; i++) {
        still[i] = (int)(base[i] * pulse * l->static_bright / 100 + 0.5);
        flash[i] = (int)(s->flash_col[i] * l->reactive_bright / 100.0 + 0.5);
        for (int k = 0; k < l->glow_n; k++) glow[k][i] = (int)(l->glow[k][i] * l->reactive_bright / 100.0 + 0.5);
    }
    mix_colour(out, flash, glow, l->glow_n, still, level, bright);
    return running;
}

/* Whether the light between key presses changes on its own, so the keyboard needs updating every frame. */
static inline int light_moving(const Lighting *l)
{
    int e = l->st.effect, n = l->seasons_n ? 1 : l->still_n;
    if (e == STILL || e == STATIC_OFF) return 0;
    return e == RAINBOW || l->st.strength || (n > 1 && e != AURORA && e != SPARKS);
}
