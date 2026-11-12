#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "effects.h"
#include "supported_keyboards.h"

static int near(double a, double b) { return fabs(a - b) < 1e-9; }

static double fl(double t, double from, double in_ms, double out_ms, int echoes, double curve)
{
    Flash f = {in_ms, out_ms, curve, 1, echoes};
    return flash_level(&f, t, from);
}

int main(void)
{
    unsigned char p[91], c[3];

    for (int k = 0; k < KEYBOARD_COUNT; k++) {
        const Keyboard *kb = &KEYBOARDS[k];
        unsigned char teal[3] = {0, 255, 180};

        razer_set_colour(p, kb, teal);
        assert(p[0] == 0 && p[1] == 0 && p[2] == kb->txn && p[6] == 8 && p[7] == kb->cls && p[8] == kb->set_cmd);
        assert(p[9] == 0 && p[13] == 0 && p[14] == 0 && p[15] == 255 && p[16] == 180 && p[17] == 0);
        assert(p[90] == 0 && (k || p[89] == 0x4F)); /* checksum as the Ornata V3 X takes it */

        razer_show(p, kb);
        assert(p[2] == kb->txn && p[6] == 0x0C && p[7] == kb->cls && p[8] == kb->show_cmd);
        assert(p[9] == 0 && p[10] == 0 && p[11] == 0x08 && p[12] == 0);
        assert(k || p[89] == 0x09);
    }

    assert(near(fl(0, 0, 0, 100, 0, 1), 1));
    assert(near(fl(50, 0, 0, 100, 0, 1), 0.5));
    assert(near(fl(100, 0, 0, 100, 0, 1), 0));
    assert(near(fl(500, 0, 0, 100, 0, 1), 0));
    assert(near(fl(50, 0, 0, 100, 0, 2), 0.25));
    assert(near(fl(0, 0.2, 100, 100, 0, 1), 0.2));
    assert(near(fl(50, 0.2, 100, 100, 0, 1), 0.6));
    assert(near(fl(100, 0.2, 100, 100, 0, 1), 1));
    assert(near(fl(150, 0.2, 100, 100, 0, 1), 0.5));
    assert(near(fl(100, 0, 0, 100, 1, 1), 1));
    assert(near(fl(150, 0, 0, 100, 1, 1), 0.5));
    assert(near(fl(200, 0, 0, 100, 1, 1), 0));
    assert(near(fl(100, 0.5, 50, 50, 1, 1), 0));

    Flash faint = {0, 100, 1, 0.4, 2};
    assert(near(flash_length(&faint), 300));
    assert(near(flash_level(&faint, 50, 0), 0.5));
    assert(near(flash_level(&faint, 100, 0), 0.4) && near(flash_level(&faint, 150, 0), 0.2));
    assert(near(flash_level(&faint, 250, 0), 0.2) && near(flash_level(&faint, 300, 0), 0));

    assert(near(breath_pulse(0, 4000), 1) && near(breath_pulse(2000, 4000), 0));
    assert(near(breath_pulse(1000, 4000), 0.5) && near(breath_pulse(4000, 4000), 1));
    assert(near(ember_pulse(1, 0, 0.3), 1) && near(ember_pulse(1, 1, 0.3), 0.7));
    double ember = 1;
    for (int i = 0; i < 1000; i++) {
        ember = ember_pulse(ember, (i * 7919 % 101) / 100.0, 0.3);
        assert(ember >= -1e-9 && ember <= 1 + 1e-9);
    }

    assert(name_index(L"Breathe", STATIC_NAMES, STATIC_EFFECTS) == BREATHE && name_index(L"beacon", STATIC_NAMES, STATIC_EFFECTS) == BEACON);
    assert(name_index(L"heat", STATIC_NAMES, STATIC_EFFECTS) == -1);
    assert(name_index(L"daylight", STATIC_NAMES, STATIC_EFFECTS) == -1);
    assert(name_index(L"", STATIC_NAMES, STATIC_EFFECTS) == -1 && name_index(L"breath", STATIC_NAMES, STATIC_EFFECTS) == -1);
    assert(name_index(L"speed", REACTIVE_NAMES, REACTIVE_EFFECTS) == TYPING_SPEED);
    assert(reactive_index(L"Rainbow") == REACTIVE_RAINBOW && reactive_index(L"fade") == REACTIVE_FLASH);
    assert(reactive_index(L"off") == REACTIVE_OFF && name_index(L"OFF", STATIC_NAMES, STATIC_EFFECTS) == STATIC_OFF);
    assert(reactive_index(L"") == REACTIVE_FLASH && reactive_index(L"sparkle") == REACTIVE_FLASH);
    assert(reactive_index(L"Heartbeat") == REACTIVE_HEARTBEAT);
    Static st;
    assert(!static_of(-1, -1, -1, &st) && !static_of(-1, 3, 3, &st));
    assert(static_of(RAINBOW, -1, -1, &st) && st.effect == RAINBOW && st.speed == 5 && st.strength == 0);
    assert(static_of(BEACON, 12, 150, &st) && st.speed == 10 && st.strength == 100);
    assert(static_of(BREATHE, 3, -1, &st) && st.speed == 3 && st.strength == 70);
    st = (Static){BREATHE, 5, 70};
    assert(near(static_period(&st), 5000));
    st.speed = 7;
    assert(near(static_period(&st), 2500));
    assert(near(beacon_pulse(0, 3000), 1) && beacon_pulse(1500, 3000) < 0.01 && near(beacon_pulse(3000, 3000), 1));
    assert(near(heartbeat_pulse(0, 1000), 1) && heartbeat_pulse(150, 1000) < 0.1);
    assert(near(heartbeat_pulse(200, 1000), 0.6) && heartbeat_pulse(900, 1000) < 0.01);
    for (int i = 0; i < 1000; i++) {
        double a = aurora_pos(i * 137.0, 20000, 1);
        assert(a >= 0 && a < 1);
    }
    assert(near(aurora_pos(0, 20000, 1), 0) && near(aurora_pos(12345, 20000, 0), 0));
    assert(near(spark_level(0), 1) && near(spark_level(125), 0.5) && near(spark_level(250), 0) && near(spark_level(-1), 0));
    assert(near(heat_press(0), 0.15) && near(heat_press(1), 1) && near(heat_after(0.8, 0), 0.8));
    assert(fabs(heat_after(1, 1386) - 0.5) < 0.01);
    double heat = 0;
    for (int i = 0; i < 200; i++) heat = heat_press(heat_after(heat, 125));
    assert(heat > 0.6 && typing_pace(heat) > 0.99);
    double slow = 0;
    for (int i = 0; i < 200; i++) slow = heat_press(heat_after(slow, 1000));
    assert(typing_pace(slow) < 0.01 && typing_pace(0) == 0);

    int rgb[3], rgbs[3][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    rotate_hue(rgbs[0], 1 / 3.0, rgb);
    assert(rgb[0] == 0 && rgb[1] == 255 && rgb[2] == 0);
    rotate_hue(rgbs[0], -1 / 6.0, rgb);
    assert(rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 255);
    int sea[3] = {0, 255, 180}, deep[3] = {0, 40, 30};
    rotate_hue(sea, 1, rgb);
    assert(abs(rgb[1] - 255) <= 1 && abs(rgb[2] - 180) <= 1);
    rotate_hue(deep, 0.5, rgb);
    assert(rgb[0] == 40 && rgb[1] == 0);
    int grey[3] = {50, 50, 50};
    rotate_hue(grey, 0.3, rgb);
    assert(rgb[0] == 50 && rgb[2] == 50);
    cycle_colour(rgbs, 3, 0, rgb);
    assert(rgb[0] == 255 && rgb[1] == 0);
    cycle_colour(rgbs, 3, 1 / 6.0, rgb);
    assert(rgb[0] == 128 && rgb[1] == 128);
    cycle_colour(rgbs, 3, 5 / 6.0, rgb);
    assert(rgb[0] == 128 && rgb[2] == 128);
    cycle_colour(rgbs, 3, 1.0, rgb);
    assert(rgb[0] == 255);
    cycle_colour(rgbs, 1, 1 / 3.0, rgb);
    assert(rgb[1] == 255 && rgb[0] == 0);
    int fire[2][3] = {{255, 120, 0}, {200, 0, 0}};
    ramp_colour(rgbs, 3, 0, rgb);
    assert(rgb[0] == 255 && rgb[1] == 0);
    ramp_colour(rgbs, 3, 0.25, rgb);
    assert(rgb[0] == 128 && rgb[1] == 128);
    ramp_colour(rgbs, 3, 1, rgb);
    assert(rgb[2] == 255 && rgb[0] == 0);
    ramp_colour(rgbs, 1, 0.7, rgb);
    assert(rgb[0] == 255 && rgb[1] == 0);
    assert(near(pulse_colour(fire, 2, 1, 0.5, rgb), 1) && rgb[0] == 255 && rgb[1] == 120);
    assert(near(pulse_colour(fire, 2, 0, 0.5, rgb), 0.5) && rgb[0] == 200 && rgb[1] == 0);
    assert(near(pulse_colour(fire, 2, 0.5, 0, rgb), 1) && rgb[0] == 228 && rgb[1] == 60);
    assert(near(pulse_colour(fire, 1, 0, 0.8, rgb), 0.2) && rgb[0] == 255);

    press_colour(REACTIVE_FLASH, rgbs, 3, 4, 0.9, rgb);
    assert(rgb[1] == 255 && rgb[0] == 0);
    press_colour(REACTIVE_RAINBOW, rgbs, 1, 4, 0, rgb);
    assert(rgb[1] == 255 && rgb[0] == 0);
    press_colour(REACTIVE_RAINBOW, rgbs, 3, 2, 0, rgb);
    assert(rgb[0] == 128 && rgb[1] == 128);
    press_colour(REACTIVE_RAINBOW, rgbs, 3, 16, 0, rgb);
    assert(rgb[1] == 255 && rgb[0] == 0);
    press_colour(TYPING_SPEED, rgbs, 3, 7, 0, rgb);
    assert(rgb[0] == 255 && rgb[2] == 0);
    press_colour(TYPING_SPEED, rgbs, 3, 7, 1, rgb);
    assert(rgb[2] == 255 && rgb[0] == 0);
    press_colour(TYPING_SPEED, rgbs, 1, 0, 1, rgb);
    assert(rgb[0] == 255 && rgb[1] == 179);

    assert(season_of(3) == 0 && season_of(5) == 0 && season_of(6) == 1 && season_of(8) == 1);
    assert(season_of(9) == 2 && season_of(11) == 2 && season_of(12) == 3 && season_of(1) == 3 && season_of(2) == 3);

    int list[MAX_COLOURS][3];
    assert(parse_colours(L"1,2,3", list, MAX_COLOURS) == 1 && list[0][2] == 3);
    assert(parse_colours(L" 1, 2 ,3 ; 4,5,6", list, MAX_COLOURS) == 2 && list[1][0] == 4 && list[1][2] == 6);
    assert(!parse_colours(L"1,2,3;4,5,6", list, 1));
    assert(!parse_colours(L"", list, 1) && !parse_colours(L"1,2", list, 1) && !parse_colours(L"1,2,3;", list, 2));
    assert(!parse_colours(L"1,2,3x", list, 1));

    assert(!in_hours(0, 1, 7) && in_hours(1, 1, 7) && in_hours(6, 1, 7) && !in_hours(7, 1, 7));
    assert(in_hours(23, 22, 7) && in_hours(3, 22, 7) && !in_hours(12, 22, 7));
    assert(!in_hours(5, 5, 5));

    Slot sched[MAX_SLOTS];
    assert(parse_schedule(L"1-7:dim; 18 - 1 : Hephaestus ;9-17:random", sched, MAX_SLOTS) == 3);
    assert(sched[0].from == 1 && sched[0].to == 7 && !wcscmp(sched[0].target, L"dim"));
    assert(sched[1].from == 18 && sched[1].to == 1 && !wcscmp(sched[1].target, L"Hephaestus"));
    assert(!wcscmp(sched[2].target, L"random"));
    assert(slot_at(sched, 3, 3) == 0 && slot_at(sched, 3, 23) == 1 && slot_at(sched, 3, 0) == 1);
    assert(slot_at(sched, 3, 12) == 2 && slot_at(sched, 3, 17) == -1 && slot_at(sched, 3, 7) == -1);
    assert(parse_schedule(L"8-12:a;10-14:b", sched, MAX_SLOTS) == 2 && slot_at(sched, 2, 11) == 0);
    assert(parse_schedule(L"", sched, MAX_SLOTS) == 0);
    assert(parse_schedule(L"1-24:dim;x;3-4:;5-6:Nyx", sched, MAX_SLOTS) == 1 && sched[0].from == 5);
    assert(parse_schedule(L"0-1:a;1-2:b;2-3:c;3-4:d;4-5:e", sched, MAX_SLOTS) == 4 && sched[3].from == 3);

    int teal[3] = {0, 255, 180}, off[3] = {0, 0, 0}, dim[3] = {0, 40, 30};
    mix_colour(c, teal, NULL, 0, off, 1, 1);
    assert(c[0] == 0 && c[1] == 255 && c[2] == 180);
    mix_colour(c, teal, NULL, 0, off, 0, 1);
    assert(c[0] == 0 && c[1] == 0 && c[2] == 0);
    mix_colour(c, teal, NULL, 0, off, 1, 0.2);
    assert(c[1] == 51 && c[2] == 36);
    mix_colour(c, teal, NULL, 0, dim, 0, 1);
    assert(c[1] == 40 && c[2] == 30);
    mix_colour(c, teal, NULL, 0, dim, 0.5, 1);
    assert(c[1] == 148 && c[2] == 105);
    mix_colour(c, teal, NULL, 0, teal, 0, 1);
    assert(c[1] == 255 && c[2] == 180);
    int red[1][3] = {{200, 0, 0}}, white[3] = {255, 255, 255};
    mix_colour(c, white, red, 1, off, 1, 1);
    assert(c[0] == 255 && c[1] == 255);
    mix_colour(c, white, red, 1, off, 0.75, 1);
    assert(c[0] == 228 && c[1] == 128);
    mix_colour(c, white, red, 1, off, 0.5, 1);
    assert(c[0] == 200 && c[1] == 0);
    mix_colour(c, white, red, 1, off, 0.25, 1);
    assert(c[0] == 100 && c[1] == 0);
    mix_colour(c, white, red, 1, off, 0, 1);
    assert(c[0] == 0 && c[1] == 0);
    int two[2][3] = {{0, 0, 240}, {0, 120, 0}};
    mix_colour(c, white, two, 2, off, 1, 1);
    assert(c[0] == 255);
    mix_colour(c, white, two, 2, off, 2 / 3.0, 1);
    assert(c[0] == 0 && c[2] == 240);
    mix_colour(c, white, two, 2, off, 0.5, 1);
    assert(c[1] == 60 && c[2] == 120);
    mix_colour(c, white, two, 2, off, 1 / 3.0, 1);
    assert(c[1] == 120 && c[2] == 0);
    mix_colour(c, white, two, 2, off, 0, 1);
    assert(c[1] == 0);

    assert(led_to_screen(0) == 0 && led_to_screen(255) == 255 && led_to_screen(25) == 88);
    assert(screen_to_led(0) == 0 && screen_to_led(255) == 255 && screen_to_led(0xDC) == 183);
    for (int v = 0; v < 256; v++)
        assert(abs(screen_to_led(led_to_screen(v)) - v) <= 1);

    /* a red flash over black, held from 1000 ms to 1500 ms, then fading over 100 ms */
    Lighting l = {.fx = {0, 100, 1, 0.5, 0}, .st = {STILL, 5, 0}, .effect = REACTIVE_FLASH, .hold = 1,
                  .static_bright = 100, .reactive_bright = 100, .cols = {{255, 0, 0}}, .cols_n = 1, .still_n = 1};
    LightState ls = {.ss = {1}};
    assert(!light_frame(&l, &ls, 1000, 1, 1, c) && c[0] == 0);
    light_press(&l, &ls, 1000);
    assert(light_frame(&l, &ls, 1400, 1, 1, c) && c[0] == 255);
    assert(light_frame(&l, &ls, 1400, 1, 0.2, c) && c[0] == 51);
    assert(light_release(&l, &ls, 1500) && !light_release(&l, &ls, 1500));
    assert(light_frame(&l, &ls, 1550, 1, 1, c) && c[0] == 128);
    assert(!light_frame(&l, &ls, 1600, 1, 1, c) && c[0] == 0);
    l.effect = REACTIVE_OFF;
    light_press(&l, &ls, 2000);
    assert(!light_frame(&l, &ls, 2000, 1, 1, c) && c[0] == 0);
    int seasons[4][3] = {{1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}};
    memcpy(l.seasons, seasons, sizeof seasons);
    l.seasons_n = 4;
    light_frame(&l, &ls, 3000, 11, 1, c);
    assert(c[0] == 3);

    assert(!light_moving(&l));
    l.st = (Static){BREATHE, 5, 0};
    assert(!light_moving(&l));
    l.st.strength = 70;
    assert(light_moving(&l));
    l.seasons_n = 0, l.still_n = 2, l.st.strength = 0;
    assert(light_moving(&l));
    l.st.effect = AURORA;
    assert(!light_moving(&l));
    l.st.effect = RAINBOW, l.still_n = 1;
    assert(light_moving(&l));

    puts("All effect tests passed.");
    return 0;
}
