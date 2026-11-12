#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include "effects.h"
#include "version.h"

/* Also how a second "arges.exe --settings" finds the window that is already open. */
#define SETTINGS_TITLE L"Arges RGB " ARGES_VERSION L" settings"

int static_in(const wchar_t *path, const wchar_t *section, Static *s); /* arges.c */
void load_lighting(const wchar_t *path, const wchar_t *section, Lighting *l); /* arges.c */

enum { IDD_SETTINGS = 101, IDC_PROFILE = 100, IDC_STATUS, IDC_DESCRIPTION, IDC_STATIC_HELP, IDC_REACTIVE_HELP,
       IDC_SCHEDULE_HELP, IDC_FADE, IDC_DAY, IDC_PICKED, IDC_TARGET, IDC_REMOVE, IDC_RESTORE, IDC_EDIT, IDC_KEYBOARD,
       IDC_FIELD = 1000, IDC_LABEL = 2000, IDC_VALUE = 3000, IDC_STEP = 4000, IDC_SWATCH = 10000 };
enum { IDD_PROFILES = 102, IDC_LIST = 5000, IDC_NAME, IDC_GROUP, IDC_ABOUT, IDC_ADD, IDC_DELETE, IDC_HINT,
       IDC_EDIT_STATUS };
enum { SLOTS = 8 };
enum { TICK, CHOICE, COLOURS, SLIDER, STEPS };
enum { DEFAULTS_ITEM = -1, GROUP_ITEM = -2, DIM_ITEM = -3, RANDOM_ITEM = -4, OFF_ITEM = -5, NO_ITEM = -9 };

static const wchar_t *const STATIC_HELP[STATIC_EFFECTS] = {
    L"No light between key presses.",
    L"A steady colour. With several colours, each key press moves on to the next.",
    L"Slowly brightens and dims, from the first colour to the last. Strength: how far it dims.",
    L"A steady double beat. Speed: how fast it beats. Strength: how far it dims between beats.",
    L"Wavers like a glowing coal, flaring to the first colour. Strength: how much it wavers.",
    L"A short pulse every few seconds. Strength: how far it dims between pulses.",
    L"Random sparks over the first colour, in the others, or white with one colour. Strength: spark brightness.",
    L"One colour goes round the rainbow from it. Several blend from one to the next and back.",
    L"Drifts unevenly through the colours. Strength: how far it drifts."};
static const wchar_t *const REACTIVE_HELP[REACTIVE_EFFECTS] = {
    L"No flash. Key presses still move Still on to its next colour.",
    L"Each key press flashes the next colour, then fades through the afterglow.",
    L"One colour moves a step round the rainbow each press. Several step through the colours.",
    L"Slow typing flashes the first colour, fast typing the last. One colour turns whiter.",
    L"Beats while you type, faster the faster you type, then fades. Duration: longer beats slower."};

static const int DURATIONS[] = {10, 25, 50, 75, 100, 150, 200, 300, 400, 500, 700, 1000, 1500, 2000, 3000, 5000, -1};
static const int MINUTES[] = {0, 1, 2, 5, 10, 15, 20, 30, 45, 60, 90, 120, 180, 240, -1};

/* In Tab order. builtin must match the defaults in load_lighting in arges.c. */
static const struct {
    const wchar_t *key, *label, *unit, *tip, *builtin;
    int kind, general, lo, hi, lx, x, y, width;
    const wchar_t *choices;
    const wchar_t *const *names;
    const int *stops;
} FIELDS[] = {
    {L"static_effect", L"Effect", NULL, L"How the light between key presses moves. The line below says how.",
     L"still", CHOICE, 0, 0, 0, 16, 78, 38, 90,
     L"Off\0Still\0Breathe\0Heartbeat\0Ember\0Beacon\0Sparks\0Rainbow\0Aurora\0", STATIC_NAMES},
    {L"static_colour", L"Colours", NULL,
     L"The colours between key presses; the line under Effect says how the effect uses them. Black is off. "
     L"Click a colour to change it, + to add one, right-click to remove one.", L"0,0,0", COLOURS, 0, 0, 6, 16, 78, 74},
    {L"static_shuffle", L"Random order", NULL,
     L"Still takes its colours in random order, never the same twice running.", L"0", TICK, 0, 0, 1, 0, 184, 75, 58},
    {L"static_speed", L"Speed", NULL, L"How fast the effect moves, from slow to fast.", L"5", SLIDER, 0, 1, 10, 16, 74,
     94, 124},
    {L"static_strength", L"Strength", L"%",
     L"How strongly the effect changes the light; the line under Effect says what it changes. At 0 several "
     L"colours still move, without dimming.", L"0", SLIDER, 0, 0, 100, 16, 74, 110, 124},
    {L"static_brightness", L"Brightness", L"%", L"How bright the light between key presses is. At 0 it is off.", L"100",
     SLIDER, 0, 0, 100, 16, 74, 126, 124},
    {L"reactive_effect", L"Effect", NULL, L"What each key press flashes. The line below says how.", L"flash",
     CHOICE, 0, 0, 0, 16, 78, 164, 90, L"Off\0Flash\0Rainbow\0Typing speed\0Heartbeat\0", REACTIVE_NAMES},
    {L"reactive_colour", L"Colours", NULL,
     L"The colours a key press flashes, one key press each in turn; the line under Effect says how the "
     L"effect uses them. Click a colour to change it, + to add one, right-click to remove one.", L"0,255,180",
     COLOURS, 0, 0, 6, 16, 78, 200},
    {L"reactive_shuffle", L"Random order", NULL,
     L"Key presses take the colours in random order, never the same twice running.", L"0", TICK, 0, 0, 1, 0, 184, 201,
     58},
    {L"reactive_afterglow", L"Afterglow", NULL,
     L"Up to three colours every flash passes through as it fades, in order, before it settles on Static. Shows best "
     L"with a long Duration. Grey with a line: none. Right-click a colour to remove it.", L"", COLOURS, 0, 0, 3, 16, 78,
     218},
    {L"reactive_brightness", L"Brightness", L"%", L"How bright each key press flashes, afterglow included.", L"100",
     SLIDER, 0, 0, 100, 16, 74, 252, 124},
    {L"reactive_duration", L"Duration", L"ms",
     L"How long the flash takes to fade. With Heartbeat, how long each beat takes to fade: longer beats slower.",
     L"100", SLIDER, 0, 0, 0, 16, 74, 268, 124, NULL, NULL, DURATIONS},
    {L"reactive_echo", L"Echo", NULL,
     L"How many times the flash plays again after each key press, at Echo strength. 1 at 100% looks like lightning.",
     L"0", STEPS, 0, 0, 4, 16, 78, 284, 18},
    {L"reactive_echo_strength", L"Echo strength", L"%",
     L"How bright each echo is, from the faintest to as bright as the flash.", L"50", SLIDER, 0, 1, 100, 16, 74, 300,
     124},
    {L"dim", L"Dim brightness", L"%",
     L"How bright the lights are during the schedule's Dim hours, and while Dim is ticked on the tray menu. At 0 they "
     L"are off.", L"20", SLIDER, 0, 0, 100, 264, 326, 237, 110},
    {L"off_after", L"Lights off after", L"min", L"Turns the lights off after this long without typing, for every "
     L"profile. The next key press turns them back on.", L"0", SLIDER, 1, 0, 0, 264, 326, 253, 110, NULL, NULL, MINUTES},
    {L"fps", L"Frame rate", L"fps", L"How many times a second Arges updates the keyboard while the light fades or "
     L"moves, for every profile. Higher is smoother; above about 60 makes no difference.", L"45", SLIDER, 1, 5, 60,
     264, 326, 269, 110},
    {L"off_with_screen", L"Lights off while the screen is off", NULL,
     L"Turns the lights off while the screen is off, for every profile.", L"1", TICK, 1, 0, 1, 0, 264, 286, 200},
    {L"reactive_hold", L"Stay lit while a key is held", NULL,
     L"The flash stays lit while a key is held down and fades when you let go.", L"1", TICK, 0, 0, 1, 0, 264, 302, 200},
};
enum { FIELD_COUNT = sizeof FIELDS / sizeof *FIELDS };
#define IS_STATIC_PART(f) (!lstrcmpW(FIELDS[f].key, L"static_effect") || !lstrcmpW(FIELDS[f].key, L"static_speed") || \
                           !lstrcmpW(FIELDS[f].key, L"static_strength"))

enum { SWATCH = 14, DIALOG_W = 496, DIALOG_H = 340 };

static wchar_t ini[MAX_PATH], names[8192], *sections[200], profile[64];
static int section_count, loading, defaults;
static int profile_item, target_item;
static HWND ctl[FIELD_COUNT], sw[FIELD_COUNT][SLOTS], tips;
static HFONT normal_font, bold_font;
static void preview_load(void);

enum { DRAG_NONE, DRAG_NEW, DRAG_START, DRAG_END };
static Slot sched[MAX_SLOTS];
static int sched_count, picked = -1, drag, anchor;

static int field(const wchar_t *key)
{
    for (int f = 0; f < FIELD_COUNT; f++)
        if (!lstrcmpW(FIELDS[f].key, key)) return f;
    return -1;
}

static const wchar_t *editing(void) { return defaults ? L"general" : profile; }
static const wchar_t *section_of(int f) { return FIELDS[f].general ? L"general" : editing(); }

static void inherited(int f, wchar_t out[256])
{
    if (FIELDS[f].general || defaults) lstrcpyW(out, FIELDS[f].builtin);
    else GetPrivateProfileStringW(L"general", FIELDS[f].key, FIELDS[f].builtin, out, 256, ini);
}

static void value(int f, wchar_t out[256])
{
    wchar_t def[256];
    inherited(f, def);
    GetPrivateProfileStringW(section_of(f), FIELDS[f].key, def, out, 256, ini);
}

static int value_int(int f)
{
    wchar_t v[256];
    value(f, v);
    return _wtoi(v);
}

static void put(int f, const wchar_t *s)
{
    wchar_t def[256];
    inherited(f, def);
    WritePrivateProfileStringW(section_of(f), FIELDS[f].key, lstrcmpW(s, def) ? s : NULL, ini);
}

static int slider_value(int f, int pos) { return FIELDS[f].stops ? FIELDS[f].stops[pos] : pos; }
static int slider_pos(int f, int v)
{
    const int *stops = FIELDS[f].stops;
    int i = 0;
    if (!stops) return v;
    while (stops[i + 1] >= 0 && stops[i] < v) i++;
    return i;
}

static void show_value(HWND w, int f, int v)
{
    wchar_t s[32];
    if (f == field(L"off_after") && !v) lstrcpyW(s, L"Never");
    else wsprintfW(s, FIELDS[f].unit ? L"%d %s" : L"%d", v, FIELDS[f].unit);
    SetDlgItemTextW(w, IDC_VALUE + f, s);
}

static Static static_now(void)
{
    Static s;
    if ((defaults || !static_in(ini, profile, &s)) && !static_in(ini, L"general", &s)) s = (Static){STILL, 5, 0};
    return s;
}

static void put_static(Static s)
{
    const wchar_t *section = editing();
    wchar_t v[16];
    WritePrivateProfileStringW(section, L"static_effect", STATIC_NAMES[s.effect], ini);
    wsprintfW(v, L"%d", s.speed);
    WritePrivateProfileStringW(section, L"static_speed", v, ini);
    wsprintfW(v, L"%d", s.strength);
    WritePrivateProfileStringW(section, L"static_strength", v, ini);
}

static int needs_colour(int f) { return lstrcmpW(FIELDS[f].key, L"reactive_afterglow") != 0; }

static int colours_of(int f, int list[MAX_COLOURS][3])
{
    wchar_t s[256];
    value(f, s);
    int max = needs_colour(f) ? MAX_COLOURS : 3;
    int n = parse_colours(s, list, max);
    return n || (!*s && !needs_colour(f)) ? n : parse_colours(FIELDS[f].builtin, list, MAX_COLOURS);
}

static int swatch(int f, int k, int v[3])
{
    int list[MAX_COLOURS][3], n = colours_of(f, list);
    if (k < n) {
        memcpy(v, list[k], sizeof list[0]);
        return 1;
    }
    return n ? 0 : -1;
}

static int slots(int f)
{
    return FIELDS[f].kind == COLOURS ? FIELDS[f].hi : FIELDS[f].kind == STEPS ? FIELDS[f].hi - FIELDS[f].lo + 1 : 0;
}

static int differs(int f, Static s)
{
    wchar_t mine[256], def[256];
    if (defaults || FIELDS[f].general) return 0;
    if (IS_STATIC_PART(f)) {
        Static d;
        if (!static_in(ini, L"general", &d)) d = (Static){STILL, 5, 0};
        return f == field(L"static_effect") ? s.effect != d.effect
             : f == field(L"static_speed") ? s.effect != d.effect || s.speed != d.speed
             : s.effect != d.effect || s.strength != d.strength;
    }
    value(f, mine);
    inherited(f, def);
    if (f == field(L"reactive_effect")) return reactive_index(mine) != reactive_index(def);
    return lstrcmpiW(mine, def) != 0;
}

static void enable_field(HWND w, int f, int on)
{
    if (ctl[f]) EnableWindow(ctl[f], on);
    EnableWindow(GetDlgItem(w, IDC_LABEL + f), on);
    EnableWindow(GetDlgItem(w, IDC_VALUE + f), on);
    for (int k = 0; k < slots(f); k++) EnableWindow(sw[f][k], on);
}

static void refresh(HWND w)
{
    Static s = static_now();
    wchar_t effect[256];
    value(field(L"reactive_effect"), effect);
    int flash = reactive_index(effect) != REACTIVE_OFF, lit = s.effect != STATIC_OFF;
    int beat = reactive_index(effect) == REACTIVE_HEARTBEAT;
    loading = 1; /* so setting a control doesn't save it */
    for (int f = 0; f < FIELD_COUNT; f++) {
        wchar_t v[256], now[256];
        int list[MAX_COLOURS][3], n;
        value(f, v);
        switch (FIELDS[f].kind) {
        case TICK: SendMessageW(ctl[f], BM_SETCHECK, _wtoi(v) != 0, 0); break;
        case CHOICE:
            SendMessageW(ctl[f], CB_SETCURSEL, f == field(L"static_effect") ? s.effect : reactive_index(v), 0);
            break;
        case SLIDER: {
            int pos = IS_STATIC_PART(f) ? (f == field(L"static_speed") ? s.speed : s.strength) : slider_pos(f, _wtoi(v));
            SendMessageW(ctl[f], TBM_SETPOS, TRUE, pos);
            show_value(w, f, FIELDS[f].stops ? _wtoi(v) : (int)SendMessageW(ctl[f], TBM_GETPOS, 0, 0));
            break;
        }
        case STEPS:
            for (int k = 0; k < slots(f); k++) SendMessageW(sw[f][k], BM_SETCHECK, k == _wtoi(v) - FIELDS[f].lo, 0);
            break;
        case COLOURS:
            n = colours_of(f, list);
            for (int k = 0; k < slots(f); k++) {
                ShowWindow(sw[f][k], k <= n ? SW_SHOW : SW_HIDE);
                wsprintfW(now, k < n ? L"%s %d" : L"Add to %s", FIELDS[f].label, k + 1);
                SetWindowTextW(sw[f][k], now);
                InvalidateRect(sw[f][k], NULL, TRUE);
            }
            break;
        }
        HWND label = FIELDS[f].kind == TICK ? ctl[f] : GetDlgItem(w, IDC_LABEL + f);
        SendMessageW(label, WM_SETFONT, (WPARAM)(differs(f, s) ? bold_font : normal_font), TRUE);
        int on = 1;
        if (!wcsncmp(FIELDS[f].key, L"static_", 7) && f != field(L"static_effect")) on = lit;
        if (f == field(L"static_speed")) on = STATIC_PERIOD[s.effect] != 0;
        if (f == field(L"static_strength")) on = STATIC_STRENGTH[s.effect] != 0;
        if (!wcsncmp(FIELDS[f].key, L"reactive_", 9) && f != field(L"reactive_effect")) on = flash;
        if (f == field(L"reactive_echo")) on = flash && !beat;
        if (f == field(L"reactive_echo_strength")) on = flash && !beat && value_int(field(L"reactive_echo")) > 0;
        enable_field(w, f, on);
    }
    SetDlgItemTextW(w, IDC_STATIC_HELP, STATIC_HELP[s.effect]);
    SetDlgItemTextW(w, IDC_REACTIVE_HELP, REACTIVE_HELP[reactive_index(effect)]);
    EnableWindow(GetDlgItem(w, IDC_FADE), flash);
    InvalidateRect(GetDlgItem(w, IDC_FADE), NULL, TRUE);
    InvalidateRect(GetDlgItem(w, IDC_DAY), NULL, FALSE);
    preview_load();
    loading = 0;
}

static int scheduled_now(void)
{
    SYSTEMTIME t;
    wchar_t keys[4];
    GetLocalTime(&t);
    int r = slot_at(sched, sched_count, t.wHour);
    if (r < 0 || !lstrcmpiW(sched[r].target, L"dim") || !lstrcmpiW(sched[r].target, L"off")) return 0;
    return !lstrcmpiW(sched[r].target, L"random")
        || (lstrcmpiW(sched[r].target, profile) && GetPrivateProfileStringW(sched[r].target, NULL, L"", keys, 4, ini));
}

static void saved(HWND w, int f)
{
    wchar_t msg[160];
    refresh(w);
    if (FIELDS[f].general) lstrcpyW(msg, L"Saved for every profile.");
    else if (defaults) lstrcpyW(msg, L"Saved to Default, for every profile that sets nothing of its own.");
    else if (scheduled_now())
        wsprintfW(msg, L"Saved to %s. The keyboard shows the scheduled profile until its hours end.", profile);
    else wsprintfW(msg, L"Saved to %s. Type on the keyboard to see it.", profile);
    SetDlgItemTextW(w, IDC_STATUS, msg);
}

static HWND add(HWND w, const wchar_t *cls, const wchar_t *text, DWORD style, int id, int x, int y, int cx, int cy)
{
    RECT r = {x, y, x + cx, y + cy};
    MapDialogRect(w, &r);
    DWORD sunken = !lstrcmpW(cls, L"Edit") || !lstrcmpW(cls, L"ListBox") ? WS_EX_CLIENTEDGE : 0;
    HWND h = CreateWindowExW(sunken, cls, text, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right - r.left,
                             r.bottom - r.top, w, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(h, WM_SETFONT, SendMessageW(w, WM_GETFONT, 0, 0), FALSE);
    if (id >= 0 && id < IDC_LIST) {
        TTTOOLINFOW t = {sizeof t, TTF_IDISHWND | TTF_SUBCLASS, w, (UINT_PTR)h};
        t.lpszText = LPSTR_TEXTCALLBACKW;
        SendMessageW(tips, TTM_ADDTOOLW, 0, (LPARAM)&t);
    }
    return h;
}

static void describe(HWND w)
{
    wchar_t d[256];
    if (defaults) lstrcpyW(d, L"Every profile uses these wherever it sets nothing of its own.");
    else GetPrivateProfileStringW(profile, L"description", L"", d, 256, ini);
    SetDlgItemTextW(w, IDC_DESCRIPTION, d);
}

static void add_item(HWND combo, const wchar_t *text, int data)
{
    int i = (int)SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)text);
    SendMessageW(combo, CB_SETITEMDATA, i, data);
}

static void fill_profile_list(HWND combo, int for_schedule)
{
    enum { MAX_GROUPS = 16 };
    wchar_t group[200][32], seen[MAX_GROUPS][32];
    int groups = 0;
    if (for_schedule) add_item(combo, L"Dim", DIM_ITEM), add_item(combo, L"Off", OFF_ITEM), add_item(combo, L"Random", RANDOM_ITEM);
    else add_item(combo, L"Default", DEFAULTS_ITEM);
    for (int k = 0; k < section_count; k++) {
        GetPrivateProfileStringW(sections[k], L"group", L"", group[k], 32, ini);
        int g = 0;
        while (g < groups && lstrcmpiW(group[k], seen[g])) g++;
        if (*group[k] && g == groups && groups < MAX_GROUPS) lstrcpyW(seen[groups++], group[k]);
        if (!*group[k] || g == MAX_GROUPS) add_item(combo, sections[k], k);
    }
    for (int g = 0; g < groups; g++) {
        add_item(combo, seen[g], GROUP_ITEM);
        for (int k = 0; k < section_count; k++)
            if (!lstrcmpiW(group[k], seen[g])) add_item(combo, sections[k], k);
    }
}

static int item_of(HWND combo, int data)
{
    for (int i = 0, n = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0); i < n; i++)
        if ((int)SendMessageW(combo, CB_GETITEMDATA, i, 0) == data) return i;
    return -1;
}

static int pick_item(HWND combo, int last)
{
    int i = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if ((int)SendMessageW(combo, CB_GETITEMDATA, i, 0) == GROUP_ITEM) {
        i += i < last ? -1 : 1;
        SendMessageW(combo, CB_SETCURSEL, i, 0);
    }
    return i;
}

static void draw_profile_item(const DRAWITEMSTRUCT *d)
{
    wchar_t s[64] = L"";
    int data = (int)d->itemData, group = data == GROUP_ITEM;
    int lit = (d->itemState & ODS_SELECTED) && !group, in_field = (d->itemState & ODS_COMBOBOXEDIT) != 0;
    RECT r = d->rcItem, indent = {0, 0, data >= 0 && !in_field ? 10 : 2, 0};
    if (d->itemID == (UINT)-1) return;
    SendMessageW(d->hwndItem, CB_GETLBTEXT, d->itemID, (LPARAM)s);
    MapDialogRect(GetParent(d->hwndItem), &indent);
    FillRect(d->hDC, &r, GetSysColorBrush(lit ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    SetBkMode(d->hDC, TRANSPARENT);
    SetTextColor(d->hDC, GetSysColor(lit ? COLOR_HIGHLIGHTTEXT : IsWindowEnabled(d->hwndItem) ? COLOR_WINDOWTEXT
                                                                                             : COLOR_GRAYTEXT));
    HGDIOBJ old = SelectObject(d->hDC, group ? bold_font : normal_font);
    r.left += indent.right;
    DrawTextW(d->hDC, s, -1, &r, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(d->hDC, old);
    if ((d->itemState & ODS_FOCUS) && !group) DrawFocusRect(d->hDC, &d->rcItem);
}

static int wrap24(int h) { return (h % 24 + 24) % 24; }
static int span(int from, int to) { return wrap24(to - from); }

static const wchar_t *shown_name(const wchar_t *target)
{
    return !lstrcmpiW(target, L"dim") ? L"Dim" : !lstrcmpiW(target, L"off") ? L"Off"
         : !lstrcmpiW(target, L"random") ? L"Random" : target;
}

static void show_picked(HWND w)
{
    wchar_t s[64];
    HWND target = GetDlgItem(w, IDC_TARGET);
    int data = NO_ITEM;
    loading = 1;
    if (picked >= 0) {
        const wchar_t *t = sched[picked].target;
        wsprintfW(s, L"%02d:00 to %02d:00", sched[picked].from, sched[picked].to);
        SetDlgItemTextW(w, IDC_PICKED, s);
        data = !lstrcmpiW(t, L"dim") ? DIM_ITEM : !lstrcmpiW(t, L"off") ? OFF_ITEM : !lstrcmpiW(t, L"random") ? RANDOM_ITEM : data;
        for (int k = 0; k < section_count; k++)
            if (!lstrcmpiW(t, sections[k])) data = k;
    } else SetDlgItemTextW(w, IDC_PICKED, L"Drag across the hours to add one.");
    SendMessageW(target, CB_SETCURSEL, target_item = item_of(target, data), 0);
    EnableWindow(target, picked >= 0);
    EnableWindow(GetDlgItem(w, IDC_REMOVE), picked >= 0);
    InvalidateRect(GetDlgItem(w, IDC_DAY), NULL, FALSE);
    loading = 0;
}

static void write_schedule(const Slot *slot, int n)
{
    wchar_t s[512] = L"";
    for (int r = 0; r < n; r++)
        wsprintfW(s + lstrlenW(s), L"%s%d-%d:%s", *s ? L";" : L"", slot[r].from, slot[r].to, slot[r].target);
    WritePrivateProfileStringW(L"general", L"schedule", s, ini);
}

static void save_schedule(HWND w)
{
    write_schedule(sched, sched_count);
    show_picked(w);
    SetDlgItemTextW(w, IDC_STATUS, L"Saved for every profile.");
}

static void add_stretch(HWND w, int h)
{
    if (sched_count == MAX_SLOTS) {
        SetDlgItemTextW(w, IDC_STATUS, L"The schedule holds four stretches. Remove one to add another.");
        return;
    }
    Slot s = {h, wrap24(h + 1)};
    lstrcpyW(s.target, L"dim");
    sched[sched_count] = s;
    picked = sched_count++;
}

static void remove_picked(HWND w)
{
    if (picked < 0) return;
    memmove(&sched[picked], &sched[picked + 1], (sched_count - picked - 1) * sizeof *sched);
    sched_count--;
    if (picked >= sched_count) picked = sched_count - 1;
    save_schedule(w);
}

static void target_picked(HWND w)
{
    HWND target = GetDlgItem(w, IDC_TARGET);
    int k = (int)SendMessageW(target, CB_GETITEMDATA, target_item = pick_item(target, target_item), 0);
    const wchar_t *t = k == DIM_ITEM ? L"dim" : k == OFF_ITEM ? L"off" : k == RANDOM_ITEM ? L"random"
                     : k >= 0 ? sections[k] : NULL;
    if (t && picked >= 0 && lstrcmpW(t, sched[picked].target)) {
        lstrcpynW(sched[picked].target, t, 64);
        save_schedule(w);
    }
}

static int hour_at(HWND bar, int x)
{
    RECT c;
    GetClientRect(bar, &c);
    return (int)floor(x * 24.0 / c.right);
}

static int edge_near(HWND bar, int x)
{
    RECT c;
    GetClientRect(bar, &c);
    return (int)floor(x * 24.0 / c.right + 0.5);
}

static int edge_at(HWND bar, int x, int *stretch)
{
    RECT c;
    GetClientRect(bar, &c);
    int b = edge_near(bar, x), bx = b * c.right / 24;
    int before = slot_at(sched, sched_count, wrap24(b - 1)), after = slot_at(sched, sched_count, wrap24(b));
    int ends = before >= 0 && sched[before].to == wrap24(b), starts = after >= 0 && sched[after].from == wrap24(b);
    if (abs(x - bx) > c.right / 96 + 3 || (!ends && !starts)) return DRAG_NONE;
    if (ends && (!starts || before == picked || (after != picked && x < bx))) {
        *stretch = before;
        return DRAG_END;
    }
    *stretch = after;
    return DRAG_START;
}

static void press(HWND w, HWND bar, int x)
{
    int h = hour_at(bar, x), s;
    drag = edge_at(bar, x, &s);
    if (drag) picked = s;
    else if ((s = slot_at(sched, sched_count, wrap24(h))) >= 0) picked = s;
    else {
        int before = sched_count;
        add_stretch(w, wrap24(h));
        if (sched_count > before) drag = DRAG_NEW, anchor = wrap24(h);
    }
    show_picked(w);
}

/* An edge moves a little at a time, so a stretch can't flip inside out. */
static void drag_to(HWND w, HWND bar, int x)
{
    Slot *s = &sched[picked];
    int h = hour_at(bar, x), b = wrap24(edge_near(bar, x)), old = span(s->from, s->to);
    if (drag == DRAG_NEW) {
        if (h > anchor + 22) h = anchor + 22; /* from == to would mean never */
        if (h < anchor - 22) h = anchor - 22;
        s->from = wrap24(h < anchor ? h : anchor);
        s->to = wrap24((h > anchor ? h : anchor) + 1);
    } else if (drag == DRAG_END && span(s->from, b) && abs(span(s->from, b) - old) <= 12) {
        s->to = b;
    } else if (drag == DRAG_START && span(b, s->to) && abs(span(b, s->to) - old) <= 12) {
        s->from = b;
    }
    show_picked(w);
}

static void bar_key(HWND w, int vk)
{
    int shift = GetKeyState(VK_SHIFT) < 0, h = 0;
    if (vk == VK_INSERT) {
        while (h < 24 && slot_at(sched, sched_count, h) >= 0) h++;
        if (h < 24) add_stretch(w, h), save_schedule(w);
    } else if (vk == VK_DELETE) {
        remove_picked(w);
    } else if ((vk == VK_UP || vk == VK_DOWN) && sched_count) {
        picked = picked < 0 ? 0 : (picked + (vk == VK_DOWN ? 1 : sched_count - 1)) % sched_count;
        show_picked(w);
    } else if ((vk == VK_LEFT || vk == VK_RIGHT) && picked >= 0) {
        Slot *s = &sched[picked];
        int step = vk == VK_RIGHT ? 1 : -1, from = shift ? wrap24(s->from + step) : s->from;
        int to = shift ? s->to : wrap24(s->to + step), n = span(from, to);
        if (n && abs(n - span(s->from, s->to)) == 1) s->from = from, s->to = to, save_schedule(w);
    }
}

static HBRUSH stretch_brush(const wchar_t *target, COLORREF *text, COLORREF *edge)
{
    wchar_t general[256], s[256];
    int c[MAX_COLOURS][3], dim = !lstrcmpiW(target, L"dim");
    *text = RGB(255, 255, 255);
    *edge = CLR_INVALID;
    if (!lstrcmpiW(target, L"off")) return CreateSolidBrush(GetSysColor(COLOR_WINDOWTEXT));
    *text = GetSysColor(COLOR_WINDOWTEXT);
    if (!lstrcmpiW(target, L"random")) return CreateHatchBrush(HS_BDIAGONAL, GetSysColor(COLOR_BTNSHADOW));
    GetPrivateProfileStringW(L"general", L"reactive_colour", L"0,255,180", general, 256, ini);
    GetPrivateProfileStringW(dim ? profile : target, L"reactive_colour", general, s, 256, ini);
    if (!parse_colours(s, c, MAX_COLOURS)) return CreateSolidBrush(GetSysColor(dim ? COLOR_BTNSHADOW : COLOR_WINDOW));
    int r = led_to_screen(min(c[0][0], 255)), g = led_to_screen(min(c[0][1], 255)), b = led_to_screen(min(c[0][2], 255));
    if (dim) *edge = RGB(r, g, b), r = r * 2 / 5, g = g * 2 / 5, b = b * 2 / 5;
    *text = r * 299 + g * 587 + b * 114 > 140000 ? RGB(20, 20, 20) : RGB(255, 255, 255);
    return CreateSolidBrush(RGB(r, g, b));
}

static void paint_day(HWND bar, HDC out)
{
    RECT c;
    GetClientRect(bar, &c);
    HDC dc = CreateCompatibleDC(out);
    HBITMAP bmp = CreateCompatibleBitmap(out, c.right, c.bottom);
    HGDIOBJ old_bmp = SelectObject(dc, bmp), old_font = SelectObject(dc, normal_font);
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    int width = c.right, height = c.bottom - tm.tmHeight - 4;
#define X(h) ((h) * width / 24)
    FillRect(dc, &c, GetSysColorBrush(COLOR_BTNFACE));
    SetBkColor(dc, GetSysColor(COLOR_WINDOW));
    for (int h = 0; h < 24;) {
        int s = slot_at(sched, sched_count, h), end = h;
        while (end < 24 && slot_at(sched, sched_count, end) == s) end++;
        RECT r = {X(h), 0, X(end), height};
        if (s < 0) {
            FillRect(dc, &r, GetSysColorBrush(COLOR_WINDOW));
            for (int k = h + 1; k < end; k++) {
                RECT line = {X(k), 1, X(k) + 1, height - 1};
                FillRect(dc, &line, GetSysColorBrush(COLOR_BTNFACE));
            }
        } else {
            COLORREF text, edge;
            HBRUSH b = stretch_brush(sched[s].target, &text, &edge);
            FillRect(dc, &r, b);
            DeleteObject(b);
            if (edge != CLR_INVALID) {
                b = CreateSolidBrush(edge);
                FrameRect(dc, &r, b);
                DeleteObject(b);
            }
            const wchar_t *name = shown_name(sched[s].target);
            SIZE size;
            GetTextExtentPoint32W(dc, name, lstrlenW(name), &size);
            if (size.cx + 6 <= r.right - r.left) {
                SetTextColor(dc, text);
                SetBkMode(dc, TRANSPARENT);
                DrawTextW(dc, name, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            if (s == picked) {
                RECT f = r;
                FrameRect(dc, &f, GetSysColorBrush(COLOR_HIGHLIGHT));
                InflateRect(&f, -1, -1);
                FrameRect(dc, &f, GetSysColorBrush(COLOR_HIGHLIGHT));
            }
        }
        h = end;
    }
    RECT frame = {0, 0, width, height};
    FrameRect(dc, &frame, GetSysColorBrush(COLOR_BTNSHADOW));
    SYSTEMTIME t;
    GetLocalTime(&t);
    int now = (t.wHour * 60 + t.wMinute) * width / 1440;
    RECT now_line = {now, 0, now + 2, height};
    FillRect(dc, &now_line, GetSysColorBrush(COLOR_WINDOWTEXT));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
    for (int h = 0; h <= 24; h++) {
        RECT tick = {X(h) - (h == 24), height, X(h) - (h == 24) + 1, height + (h % 6 ? 2 : 4)};
        FillRect(dc, &tick, GetSysColorBrush(COLOR_BTNSHADOW));
        if (h % 6) continue;
        wchar_t label[8];
        wsprintfW(label, L"%02d:00", h);
        RECT r = {X(h) - 40, height + 3, X(h) + 40, c.bottom};
        if (h == 0) r.left = 0, r.right = 80;
        if (h == 24) r.left = width - 80, r.right = width;
        DrawTextW(dc, label, -1, &r, (h == 0 ? DT_LEFT : h == 24 ? DT_RIGHT : DT_CENTER) | DT_SINGLELINE);
    }
    if (GetFocus() == bar) {
        RECT f = {2, 2, width - 2, height - 2};
        DrawFocusRect(dc, &f);
    }
#undef X
    BitBlt(out, 0, 0, c.right, c.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old_font);
    SelectObject(dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

static LRESULT CALLBACK day_proc(HWND bar, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND w = GetParent(bar);
    int s;
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        paint_day(bar, BeginPaint(bar, &ps));
        EndPaint(bar, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_GETDLGCODE: return DLGC_WANTARROWS;
    case WM_SETFOCUS:
    case WM_KILLFOCUS: InvalidateRect(bar, NULL, FALSE); return 0;
    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(bar, &pt);
        if (LOWORD(lp) != HTCLIENT || !edge_at(bar, pt.x, &s)) break;
        SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_SIZEWE));
        return TRUE;
    }
    case WM_LBUTTONDOWN:
        SetFocus(bar);
        SetCapture(bar);
        press(w, bar, (short)LOWORD(lp));
        return 0;
    case WM_MOUSEMOVE:
        if (GetCapture() == bar && drag && (wp & MK_LBUTTON)) drag_to(w, bar, (short)LOWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == bar) ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        if (drag) drag = DRAG_NONE, save_schedule(w);
        return 0;
    case WM_KEYDOWN: bar_key(w, (int)wp); return 0;
    }
    return DefWindowProcW(bar, msg, wp, lp);
}

static void restore(HWND w)
{
    static const wchar_t *const KEEP[] = {L"profile", L"random", L"schedule", L"off_after", L"off_with_screen", L"fps"};
    enum { KEPT = sizeof KEEP / sizeof *KEEP };
    wchar_t dir[MAX_PATH], tmp[MAX_PATH], msg[200], keys[8192], kept[KEPT][512];
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(3), (LPCWSTR)RT_RCDATA);
    DWORD written;
    GetTempPathW(MAX_PATH, dir);
    GetTempFileNameW(dir, L"arg", 0, tmp);
    HANDLE f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        if (r) WriteFile(f, LockResource(LoadResource(NULL, r)), SizeofResource(NULL, r), &written, NULL);
        CloseHandle(f);
    }
    if (!GetPrivateProfileSectionW(editing(), keys, 8192, tmp)) {
        wsprintfW(msg, L"Not restored. %s didn't come with Arges, so there is nothing to put back.", profile);
        SetDlgItemTextW(w, IDC_STATUS, msg);
    } else {
        wsprintfW(msg, defaults ? L"Put Default back as it came? The schedule, the lights off settings and the frame rate "
                                  L"stay as they are."
                                : L"Put %s back as it came? Your changes to it are lost.", profile);
        if (MessageBoxW(w, msg, L"Restore defaults", MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
            for (int k = 0; defaults && k < KEPT; k++) GetPrivateProfileStringW(L"general", KEEP[k], L"\1", kept[k], 512, ini);
            WritePrivateProfileSectionW(editing(), keys, ini);
            for (int k = 0; defaults && k < KEPT; k++)
                WritePrivateProfileStringW(L"general", KEEP[k], *kept[k] == 1 ? NULL : kept[k], ini);
            refresh(w);
            describe(w);
            wsprintfW(msg, defaults ? L"Restored Default." : L"Restored %s.", profile);
            SetDlgItemTextW(w, IDC_STATUS, msg);
        }
    }
    DeleteFileW(tmp);
}

static void build(HWND w)
{
    tips = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, 0, 0, 0, 0,
                           w, NULL, NULL, NULL);
    RECT tip_w = {0, 0, 200, 0};
    MapDialogRect(w, &tip_w);
    SendMessageW(tips, TTM_SETMAXTIPWIDTH, 0, tip_w.right);
    SendMessageW(tips, TTM_SETDELAYTIME, TTDT_AUTOPOP, 30000);
    LOGFONTW lf;
    normal_font = (HFONT)SendMessageW(w, WM_GETFONT, 0, 0);
    GetObjectW(normal_font, sizeof lf, &lf);
    lf.lfWeight = FW_BOLD;
    bold_font = CreateFontIndirectW(&lf);
    const DWORD LIST = CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP | WS_GROUP;

    add(w, L"Static", L"Profile", 0, -1, 8, 9, 40, 10);
    HWND p = add(w, L"ComboBox", L"", LIST, IDC_PROFILE, 40, 7, 120, 220);
    fill_profile_list(p, 0);
    for (int k = 0; k < section_count; k++)
        if (!lstrcmpiW(sections[k], profile)) SendMessageW(p, CB_SETCURSEL, profile_item = item_of(p, k), 0);
    add(w, L"Static", L"", SS_ENDELLIPSIS, IDC_DESCRIPTION, 168, 9, 178, 10);
    add(w, L"Button", L"Edit profiles", BS_PUSHBUTTON | WS_TABSTOP | WS_GROUP, IDC_EDIT, 350, 6, 58, 14);
    add(w, L"Button", L"Restore defaults", BS_PUSHBUTTON | WS_TABSTOP | WS_GROUP, IDC_RESTORE, 412, 6, 76, 14);

    add(w, L"Button", L"Static (idle between key presses)", BS_GROUPBOX, -1, 8, 26, 240, 120);
    add(w, L"Static", L"", 0, IDC_STATIC_HELP, 16, 53, 224, 20);
    add(w, L"Button", L"Reactive (on each key press)", BS_GROUPBOX, -1, 8, 152, 240, 170);
    add(w, L"Static", L"", 0, IDC_REACTIVE_HELP, 16, 179, 224, 20);
    add(w, L"Static", L"Fade", 0, -1, 16, 238, 58, 10);
    add(w, L"Static", L"", SS_OWNERDRAW | SS_NOTIFY, IDC_FADE, 78, 236, 162, 12);
    add(w, L"Button", L"Schedule (for every profile)", BS_GROUPBOX, -1, 256, 117, 232, 102);
    add(w, L"Static", L"Drag across the hours to dim or turn off the lights, or switch to another profile or a "
        L"random one, at those hours every day.", 0, IDC_SCHEDULE_HELP, 264, 129, 216, 18);
    HWND day = add(w, L"ArgesDay", L"Schedule", WS_TABSTOP | WS_GROUP, IDC_DAY, 264, 149, 216, 40);
    add(w, L"Static", L"", SS_NOTIFY, IDC_PICKED, 264, 199, 60, 10);
    HWND target = add(w, L"ComboBox", L"", LIST, IDC_TARGET, 326, 197, 94, 220);
    fill_profile_list(target, 1);
    HWND remove = add(w, L"Button", L"Remove", BS_PUSHBUTTON | WS_TABSTOP | WS_GROUP, IDC_REMOVE, 424, 196, 56, 14);
    add(w, L"Button", L"Preview", BS_GROUPBOX, -1, 256, 26, 232, 85);
    add(w, L"Static", L"", SS_OWNERDRAW | SS_NOTIFY, IDC_KEYBOARD, 264, 37, 216, 67);
    add(w, L"Button", L"Lights", BS_GROUPBOX, -1, 256, 225, 232, 97);

    for (int f = 0; f < FIELD_COUNT; f++) {
        const int x = FIELDS[f].x, y = FIELDS[f].y, kind = FIELDS[f].kind;
        if (kind != TICK && FIELDS[f].label)
            add(w, L"Static", FIELDS[f].label, SS_NOTIFY, IDC_LABEL + f, FIELDS[f].lx, y + 2, x - FIELDS[f].lx - 2, 10);
        if (kind == TICK) {
            ctl[f] = add(w, L"Button", FIELDS[f].label, BS_AUTOCHECKBOX | WS_TABSTOP | WS_GROUP, IDC_FIELD + f, x, y,
                         FIELDS[f].width, 12);
        } else if (kind == CHOICE) {
            ctl[f] = add(w, L"ComboBox", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | WS_GROUP, IDC_FIELD + f, x, y,
                         FIELDS[f].width, 160);
            for (const wchar_t *k = FIELDS[f].choices; *k; k += lstrlenW(k) + 1)
                SendMessageW(ctl[f], CB_ADDSTRING, 0, (LPARAM)k);
        } else if (kind == SLIDER) {
            int hi = FIELDS[f].hi;
            if (FIELDS[f].stops)
                while (FIELDS[f].stops[hi + 1] >= 0) hi++;
            ctl[f] = add(w, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP | WS_GROUP, IDC_FIELD + f, x, y,
                         FIELDS[f].width, 13);
            SendMessageW(ctl[f], TBM_SETRANGE, FALSE, MAKELPARAM(FIELDS[f].lo, hi));
            SendMessageW(ctl[f], TBM_SETPAGESIZE, 0, hi > 20 ? 10 : 1);
            add(w, L"Static", L"", SS_NOTIFY, IDC_VALUE + f, x + FIELDS[f].width + 2, y + 2, 34, 10);
        } else if (kind == STEPS) {
            for (int k = 0; k < slots(f); k++) {
                wchar_t n[8];
                wsprintfW(n, L"%d", FIELDS[f].lo + k);
                sw[f][k] = add(w, L"Button", n, BS_AUTORADIOBUTTON | BS_PUSHLIKE | (k ? 0 : WS_TABSTOP | WS_GROUP),
                               IDC_STEP + f * SLOTS + k, x + k * (FIELDS[f].width + 2), y, FIELDS[f].width, 13);
            }
        } else {
            RECT r = {x, y, x, y + SWATCH};
            MapDialogRect(w, &r);
            int side = r.bottom - r.top;
            for (int k = 0; k < slots(f); k++) {
                sw[f][k] = add(w, L"Button", L"", BS_OWNERDRAW | WS_TABSTOP | WS_GROUP, IDC_SWATCH + f * SLOTS + k, x, y,
                               SWATCH, SWATCH);
                SetWindowPos(sw[f][k], NULL, r.left + k * side * 5 / 4, r.top, side, side, SWP_NOZORDER);
            }
        }
    }
    /* Tab order: the schedule between Reactive and Lights */
    SetWindowPos(day, ctl[field(L"reactive_echo_strength")], 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(target, day, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(remove, target, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    add(w, L"Static", L"", 0, IDC_STATUS, 8, DIALOG_H - 12, DIALOG_W - 16, 10);

    wchar_t s[512];
    GetPrivateProfileStringW(L"general", L"schedule", L"", s, 512, ini);
    sched_count = parse_schedule(s, sched, MAX_SLOTS);
    picked = sched_count ? 0 : -1;
    show_picked(w);

    RECT r = {0, 0, DIALOG_W, DIALOG_H};
    MapDialogRect(w, &r);
    AdjustWindowRectEx(&r, GetWindowLongW(w, GWL_STYLE), FALSE, GetWindowLongW(w, GWL_EXSTYLE));
    int cx = r.right - r.left, cy = r.bottom - r.top;
    SetWindowPos(w, NULL, (GetSystemMetrics(SM_CXSCREEN) - cx) / 2, (GetSystemMetrics(SM_CYSCREEN) - cy) / 2, cx, cy,
                 SWP_NOZORDER);
}

static void store_colours(HWND w, int f, int list[][3], int n)
{
    wchar_t s[256] = L"";
    for (int k = 0; k < n; k++)
        wsprintfW(s + lstrlenW(s), k ? L";%d,%d,%d" : L"%d,%d,%d", list[k][0], list[k][1], list[k][2]);
    put(f, s);
    saved(w, f);
}

static void pick(HWND w, int f, int k)
{
    static COLORREF custom[16];
    int list[MAX_COLOURS][3], v[3] = {0}, n = 0, kind = swatch(f, k, v);
    if (kind <= 0 && (n = colours_of(f, list))) memcpy(v, list[n - 1], sizeof v);
    CHOOSECOLORW cc = {sizeof cc};
    cc.hwndOwner = w;
    cc.rgbResult = RGB(led_to_screen(v[0]), led_to_screen(v[1]), led_to_screen(v[2]));
    cc.lpCustColors = custom;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_ANYCOLOR;
    COLORREF start = cc.rgbResult;
    if (!ChooseColorW(&cc) || (kind > 0 && cc.rgbResult == start)) return; /* the screen-LED round trip is lossy */
    n = colours_of(f, list);
    int *to = list[k < n ? k : n];
    to[0] = screen_to_led(GetRValue(cc.rgbResult));
    to[1] = screen_to_led(GetGValue(cc.rgbResult));
    to[2] = screen_to_led(GetBValue(cc.rgbResult));
    store_colours(w, f, list, k < n ? n : n + 1);
}

static void unpick(HWND w, int f, int k)
{
    int list[MAX_COLOURS][3], n = colours_of(f, list);
    if (k >= n) return;
    if (n == 1 && needs_colour(f)) {
        memset(list, 0, sizeof list[0]);
        store_colours(w, f, list, 1);
        return;
    }
    memmove(list[k], list[k + 1], (n - k - 1) * sizeof list[0]);
    store_colours(w, f, list, n - 1);
}

static void draw_swatch(const DRAWITEMSTRUCT *d)
{
    int id = d->CtlID - IDC_SWATCH, v[3] = {0}, kind = swatch(id / SLOTS, id % SLOTS, v);
    RECT r = d->rcItem;
    FillRect(d->hDC, &r, GetSysColorBrush(COLOR_BTNFACE));
    if (kind > 0) {
        HBRUSH b = CreateSolidBrush(RGB(led_to_screen(v[0]), led_to_screen(v[1]), led_to_screen(v[2])));
        FillRect(d->hDC, &r, b);
        DeleteObject(b);
    } else if (kind < 0) {
        HGDIOBJ old = SelectObject(d->hDC, GetStockObject(DC_PEN));
        SetDCPenColor(d->hDC, GetSysColor(COLOR_BTNSHADOW));
        MoveToEx(d->hDC, r.left, r.top, NULL);
        LineTo(d->hDC, r.right, r.bottom);
        SelectObject(d->hDC, old);
    } else {
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, GetSysColor(d->itemState & ODS_DISABLED ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
        DrawTextW(d->hDC, L"+", 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    FrameRect(d->hDC, &r, GetSysColorBrush(COLOR_BTNSHADOW));
    if (d->itemState & ODS_FOCUS) {
        InflateRect(&r, -2, -2);
        DrawFocusRect(d->hDC, &r);
    }
}

static void scale(int list[][3], int n, int bright)
{
    for (int k = 0; k < n; k++)
        for (int i = 0; i < 3; i++) list[k][i] = list[k][i] * bright / 100;
}

static void draw_fade(const DRAWITEMSTRUCT *d)
{
    int cols[MAX_COLOURS][3], glow[MAX_COLOURS][3], still[MAX_COLOURS][3];
    int n = IsWindowEnabled(d->hwndItem) ? colours_of(field(L"reactive_colour"), cols) : 0;
    int g = colours_of(field(L"reactive_afterglow"), glow);
    colours_of(field(L"static_colour"), still);
    if (static_now().effect == STATIC_OFF) memset(still, 0, sizeof still[0]);
    scale(cols, n, value_int(field(L"reactive_brightness")));
    scale(glow, g, value_int(field(L"reactive_brightness")));
    scale(still, 1, value_int(field(L"static_brightness")));
    RECT r = d->rcItem;
    int width = r.right - r.left, height = r.bottom - r.top;
    FillRect(d->hDC, &r, GetSysColorBrush(COLOR_BTNFACE));
    for (int k = 0; k < n; k++)
        for (int x = 0; x < width; x++) {
            unsigned char c[3];
            mix_colour(c, cols[k], glow, g, still[0], 1 - x / (double)(width - 1), 1);
            SetDCBrushColor(d->hDC, RGB(led_to_screen(c[0]), led_to_screen(c[1]), led_to_screen(c[2])));
            RECT band = {r.left + x, r.top + k * height / n, r.left + x + 1, r.top + (k + 1) * height / n};
            FillRect(d->hDC, &band, GetStockObject(DC_BRUSH));
        }
    FrameRect(d->hDC, &r, GetSysColorBrush(COLOR_BTNSHADOW));
}

/* The Preview keyboard runs the tray app's light maths, typing a short pattern of key presses on a loop. */
enum { TIMER_KEYBOARD = 1 };
static Lighting pc;
static struct {
    LightState s;
    ULONGLONG t0;
    int pressed, ev, cyc;
    unsigned char c[3];
} pv = {.s.ss = {1}};

static void preview_load(void)
{
    load_lighting(ini, editing(), &pc);
    pv.t0 = GetTickCount64(); /* a changed setting shows from the start of the loop */
    pv.ev = pv.cyc = pv.pressed = pv.s.held = 0;
}

/* One slow tap on a loop, long enough for the flash to fade out before the next. */
enum { TAP_AT = 700, TAP_MS = 220 };

static int loop_ms(void)
{
    double flash = pc.effect == REACTIVE_HEARTBEAT ? HEART_LINGER : flash_length(&pc.fx);
    return TAP_AT + (int)fmax(2300, flash + 1500);
}

static void preview_step(void)
{
    SYSTEMTIME t;
    ULONGLONG now = GetTickCount64();
    int cyc = (int)((now - pv.t0) % loop_ms());
    if (cyc < pv.cyc) pv.ev = 0; /* the loop came round */
    pv.cyc = cyc;
    if (!pv.ev && cyc >= TAP_AT) light_press(&pc, &pv.s, (double)now), pv.ev = pv.pressed = 1;
    if (pv.pressed && cyc >= TAP_AT + TAP_MS) light_release(&pc, &pv.s, (double)now), pv.pressed = 0;
    GetLocalTime(&t);
    light_frame(&pc, &pv.s, (double)now, t.wMonth, 1, pv.c);
}

typedef struct { float x, y, w, h; } Key;
enum { MAX_KEYS = 120 };
static Key keys[MAX_KEYS];
static int key_n, tap_key;
static void key(float x, float y, float w, float h)
{
    keys[key_n++] = (Key){x, y, w, h};
}

/* A full-size board in key units, 22.5 wide and 6.5 high, like the Ornata V3 X */
static void build_keys(void)
{
    static const float ROWS[5][16] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
        {1.5f, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5f},
        {1.75f, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.25f},
        {2.25f, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.75f},
        {1.25f, 1.25f, 1.25f, 6.25f, 1.25f, 1.25f, 1.25f, 1.25f},
    };
    if (key_n) return;
    key(0, 0, 1, 1);
    for (int g = 0; g < 3; g++)
        for (int k = 0; k < 4; k++) key(2 + g * 4.5f + k, 0, 1, 1);
    for (int k = 0; k < 3; k++) key(15.25f + k, 0, 1, 1);
    for (int row = 0; row < 5; row++) {
        float x = 0;
        for (int k = 0; ROWS[row][k] > 0; k++) {
            if (row == 2 && k == 5) tap_key = key_n;
            key(x, 1.5f + row, ROWS[row][k], 1), x += ROWS[row][k];
        }
    }
    for (int k = 0; k < 3; k++) key(15.25f + k, 1.5f, 1, 1), key(15.25f + k, 2.5f, 1, 1);
    key(16.25f, 4.5f, 1, 1);
    for (int k = 0; k < 3; k++) key(15.25f + k, 6.5f - 1, 1, 1);
    for (int k = 0; k < 4; k++) key(18.5f + k, 1.5f, 1, 1);
    for (int row = 0; row < 3; row++)
        for (int k = 0; k < 3; k++) key(18.5f + k, 2.5f + row, 1, 1);
    key(21.5f, 2.5f, 1, 2);
    key(21.5f, 4.5f, 1, 2);
    key(18.5f, 5.5f, 2, 1);
    key(20.5f, 5.5f, 1, 1);
}

static COLORREF mix(const unsigned char c[3], int r, int g, int b, double k)
{
    return RGB((int)(r + (led_to_screen(c[0]) - r) * k), (int)(g + (led_to_screen(c[1]) - g) * k),
               (int)(b + (led_to_screen(c[2]) - b) * k));
}

static void round_fill(HDC dc, double l, double t, double r, double b, double radius, COLORREF c)
{
    HBRUSH brush = CreateSolidBrush(c);
    HGDIOBJ old_brush = SelectObject(dc, brush), old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, (int)l, (int)t, (int)r + 1, (int)b + 1, (int)(radius * 2), (int)(radius * 2));
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(brush);
}

/* Drawn at twice the size and shrunk, so the edges are smooth. Modelled on the Ornata V3 X: near-square dark caps
 * with the light showing in the gaps between them. */
static void draw_keyboard(const DRAWITEMSTRUCT *d)
{
    enum { SCALE = 2 };
    RECT r = d->rcItem;
    int width = r.right - r.left, height = r.bottom - r.top, w2 = width * SCALE, h2 = height * SCALE;
    build_keys();
    HDC big = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, w2, h2);
    HGDIOBJ old = SelectObject(big, bmp);
    RECT all = {0, 0, w2, h2};
    FillRect(big, &all, GetSysColorBrush(COLOR_BTNFACE));

    double u = fmin(w2 / 23.3, h2 / 7.3), pad = 0.4 * u;
    double bw = 22.5 * u, bh = 6.5 * u, ox = (w2 - bw) / 2, oy = (h2 - bh) / 2;
    round_fill(big, ox - pad, oy - pad, ox + bw + pad, oy + bh + pad, u * 0.18, RGB(56, 61, 66)); /* frame edge */
    round_fill(big, ox - pad + SCALE, oy - pad + SCALE, ox + bw + pad - SCALE, oy + bh + pad - SCALE, u * 0.16,
               RGB(27, 30, 34));
    for (int k = 0; k < 5; k++) { /* status lights above the numpad */
        double cx = ox + (18.85 + k * 0.9) * u, cy = oy + 0.32 * u, rad = 0.07 * u;
        round_fill(big, cx - rad, cy - rad, cx + rad, cy + rad, rad, RGB(96, 102, 108));
    }
    double gap = u * 0.14, ring = u * 0.06, halo = u * 0.16, radius = u * 0.06;
    for (int i = 0; i < key_n; i++) {
        double l = ox + keys[i].x * u + gap / 2, t = oy + keys[i].y * u + gap / 2;
        double rr = ox + (keys[i].x + keys[i].w) * u - gap / 2, b = oy + (keys[i].y + keys[i].h) * u - gap / 2;
        round_fill(big, l - halo, t - halo, rr + halo, b + halo, radius + halo, mix(pv.c, 27, 30, 34, 0.3));
        round_fill(big, l - ring, t - ring, rr + ring, b + ring, radius + ring, mix(pv.c, 27, 30, 34, 1));
    }
    for (int i = 0; i < key_n; i++) {
        double l = ox + keys[i].x * u + gap / 2, t = oy + keys[i].y * u + gap / 2;
        double rr = ox + (keys[i].x + keys[i].w) * u - gap / 2, b = oy + (keys[i].y + keys[i].h) * u - gap / 2;
        int down = i == tap_key && pv.pressed;
        double side = u * 0.07, sink = down ? u * 0.08 : 0;
        round_fill(big, l, t, rr, b, radius, RGB(20, 23, 26)); /* cap sides */
        round_fill(big, l + side, t + side * 0.6 + sink, rr - side, b - side * 1.8 + sink, radius,
                   down ? RGB(24, 27, 30) : RGB(42, 47, 52)); /* top face, sunk while pressed */
    }
    SetStretchBltMode(d->hDC, HALFTONE);
    SetBrushOrgEx(d->hDC, 0, 0, NULL);
    StretchBlt(d->hDC, r.left, r.top, width, height, big, 0, 0, w2, h2, SRCCOPY);
    SelectObject(big, old);
    DeleteObject(bmp);
    DeleteDC(big);
}

static const wchar_t *tip_for(int id)
{
    if (id >= IDC_SWATCH && id < IDC_SWATCH + FIELD_COUNT * SLOTS) return FIELDS[(id - IDC_SWATCH) / SLOTS].tip;
    if (id >= IDC_STEP && id < IDC_STEP + FIELD_COUNT * SLOTS) return FIELDS[(id - IDC_STEP) / SLOTS].tip;
    if (id >= IDC_FIELD && id < IDC_FIELD + FIELD_COUNT) return FIELDS[id - IDC_FIELD].tip;
    if (id >= IDC_LABEL && id < IDC_LABEL + FIELD_COUNT) return FIELDS[id - IDC_LABEL].tip;
    if (id >= IDC_VALUE && id < IDC_VALUE + FIELD_COUNT) return FIELDS[id - IDC_VALUE].tip;
    switch (id) {
    case IDC_FADE:
        return L"One key press, from left to right: the flash fading through the afterglow to Static. With several "
               L"colours, each band is one key press, top to bottom in the order they take turns.";
    case IDC_KEYBOARD:
        return L"The profile on screen, with one key press every few seconds. It follows every setting as you "
               L"change it.";
    case IDC_DAY:
        return L"For every profile. Drag across free hours to add a stretch, up to four, then choose below what it "
               L"does. Drag a stretch's edge to change its hours, past either end of the bar to go past midnight. "
               L"Where stretches overlap, the one made first wins. Keys: Up and Down pick a stretch, Left and Right "
               L"move its end, Shift with them its start, Insert adds one, Delete removes it.";
    case IDC_PICKED: return L"The hours of the stretch picked on the bar, from the first up to the second.";
    case IDC_TARGET:
        return L"What the picked stretch does: dim the profile in use, turn the lights off, switch to a random "
               L"profile, or switch to the profile picked here.";
    case IDC_REMOVE: return L"Removes the picked stretch from the schedule.";
    case IDC_EDIT: return L"Add, rename, regroup, describe or delete profiles.";
    case IDC_RESTORE:
        return L"Puts the profile you are editing back as it came with Arges. With Default picked, puts Default back, "
               L"leaving the schedule, the lights off settings and the frame rate as they are.";
    case IDC_PROFILE:
        return L"The profile you are editing, which is also the one in use; the tray menu switches profiles too. "
               L"Default: what every profile uses wherever it sets nothing of its own. Bold labels differ from "
               L"Default.";
    }
    return L"";
}

static void changed(HWND w, int f, int k)
{
    wchar_t s[256];
    int i = (int)SendMessageW(ctl[f], FIELDS[f].kind == SLIDER ? TBM_GETPOS : CB_GETCURSEL, 0, 0);
    if (IS_STATIC_PART(f)) {
        Static st = static_now();
        if (FIELDS[f].kind == CHOICE && i != st.effect) st.effect = i, st.strength = STATIC_STRENGTH[i];
        else if (f == field(L"static_speed")) st.speed = i;
        else if (f == field(L"static_strength")) st.strength = i;
        put_static(st);
        saved(w, f);
        return;
    }
    switch (FIELDS[f].kind) {
    case TICK: put(f, SendMessageW(ctl[f], BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0"); break;
    case CHOICE: put(f, FIELDS[f].names[i]); break;
    case SLIDER: wsprintfW(s, L"%d", slider_value(f, i)), put(f, s); break;
    case STEPS: wsprintfW(s, L"%d", FIELDS[f].lo + k), put(f, s); break;
    }
    saved(w, f);
}

static int edited;

static void load_sections(void)
{
    section_count = 0;
    GetPrivateProfileSectionNamesW(names, 8192, ini);
    for (wchar_t *s = names; *s && section_count < 200; s += lstrlenW(s) + 1)
        if (lstrcmpiW(s, L"general")) sections[section_count++] = s;
}

static void trim(wchar_t *s)
{
    int n = lstrlenW(s), i = 0;
    while (n && iswspace(s[n - 1])) s[--n] = 0;
    while (iswspace(s[i])) i++;
    memmove(s, s + i, (n - i + 1) * sizeof *s);
}

static const wchar_t *bad_name(const wchar_t *name, const wchar_t *self)
{
    char bytes[256];
    BOOL lossy = FALSE; /* the ini is ANSI: other characters save as ? */
    if (!*name) return L"Give it a name.";
    if (wcspbrk(name, L"[];")) return L"A name can't hold [, ] or ;.";
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, name, -1, bytes, sizeof bytes, NULL, &lossy);
    if (lossy) return L"Arges can't save some of these characters. Try other letters.";
    if (!lstrcmpiW(name, L"general") || !lstrcmpiW(name, L"Default")) return L"That name is kept for Default.";
    for (int k = 0; k < section_count; k++)
        if (lstrcmpiW(sections[k], self) && !lstrcmpiW(sections[k], name)) return L"There is already a profile with that name.";
    return NULL;
}

static void retarget(const wchar_t *old, const wchar_t *name)
{
    wchar_t s[512];
    Slot slot[MAX_SLOTS];
    GetPrivateProfileStringW(L"general", L"schedule", L"", s, 512, ini);
    int n = parse_schedule(s, slot, MAX_SLOTS), kept = 0;
    for (int r = 0; r < n; r++) {
        if (!lstrcmpiW(slot[r].target, old)) {
            if (!name) continue;
            lstrcpynW(slot[r].target, name, 64);
        }
        slot[kept++] = slot[r];
    }
    write_schedule(slot, kept);
}

static int rename_section(const wchar_t *old, const wchar_t *name)
{
    WritePrivateProfileStringW(NULL, NULL, NULL, ini); /* flush the cache first */
    HANDLE f = CreateFileW(ini, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD size = GetFileSize(f, NULL), got = 0;
    char *bytes = malloc(size + 1);
    ReadFile(f, bytes, size, &got, NULL);
    CloseHandle(f);
    int length = MultiByteToWideChar(CP_ACP, 0, bytes, (int)got, NULL, 0), n = lstrlenW(old), done = 0;
    wchar_t *text = calloc(length + 1, sizeof *text), *hit = NULL;
    MultiByteToWideChar(CP_ACP, 0, bytes, (int)got, text, length);
    for (wchar_t *line = text; line && !hit; line = wcschr(line, L'\n') ? wcschr(line, L'\n') + 1 : NULL)
        if (line[0] == L'[' && !_wcsnicmp(line + 1, old, n) && line[n + 1] == L']') hit = line;
    if (hit) {
        wchar_t *out = calloc(length + lstrlenW(name) + 1, sizeof *out);
        memcpy(out, text, (hit - text + 1) * sizeof *out);
        lstrcatW(out, name);
        lstrcatW(out, hit + n + 1);
        int out_size = WideCharToMultiByte(CP_ACP, 0, out, -1, NULL, 0, NULL, NULL);
        char *out_bytes = malloc(out_size);
        WideCharToMultiByte(CP_ACP, 0, out, -1, out_bytes, out_size, NULL, NULL);
        wchar_t tmp[MAX_PATH + 4];
        wsprintfW(tmp, L"%s.tmp", ini); /* a failed write leaves the settings file as it was */
        f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD wrote;
            done = WriteFile(f, out_bytes, out_size - 1, &wrote, NULL) && wrote == (DWORD)out_size - 1;
            CloseHandle(f);
            done = done && MoveFileExW(tmp, ini, MOVEFILE_REPLACE_EXISTING);
            if (!done) DeleteFileW(tmp);
        }
        free(out_bytes);
        free(out);
    }
    free(text);
    free(bytes);
    return done;
}

static void editor_show(HWND d)
{
    wchar_t s[256];
    SetDlgItemTextW(d, IDC_NAME, sections[edited]);
    GetPrivateProfileStringW(sections[edited], L"group", L"", s, 256, ini);
    SetDlgItemTextW(d, IDC_GROUP, s);
    GetPrivateProfileStringW(sections[edited], L"description", L"", s, 256, ini);
    SetDlgItemTextW(d, IDC_ABOUT, s);
}

static void editor_fill(HWND d, const wchar_t *select)
{
    HWND list = GetDlgItem(d, IDC_LIST), group = GetDlgItem(d, IDC_GROUP);
    wchar_t g[32];
    load_sections();
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    SendMessageW(group, CB_RESETCONTENT, 0, 0);
    edited = 0;
    for (int k = 0; k < section_count; k++) {
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)sections[k]);
        if (!lstrcmpiW(sections[k], select)) edited = k;
        GetPrivateProfileStringW(sections[k], L"group", L"", g, 32, ini);
        if (*g && SendMessageW(group, CB_FINDSTRINGEXACT, -1, (LPARAM)g) == CB_ERR) SendMessageW(group, CB_ADDSTRING, 0, (LPARAM)g);
    }
    SendMessageW(list, LB_SETCURSEL, edited, 0);
    editor_show(d);
}

static void editor_commit(HWND d)
{
    wchar_t name[64], old[64], group[32], about[256], was[256], msg[200];
    if (edited >= section_count) return;
    lstrcpynW(old, sections[edited], 64);
    GetDlgItemTextW(d, IDC_GROUP, group, 32);
    trim(group);
    GetPrivateProfileStringW(old, L"group", L"", was, 256, ini);
    int group_changed = lstrcmpW(group, was) != 0;
    if (group_changed) WritePrivateProfileStringW(old, L"group", *group ? group : NULL, ini);
    GetDlgItemTextW(d, IDC_ABOUT, about, 256);
    trim(about);
    GetPrivateProfileStringW(old, L"description", L"", was, 256, ini);
    if (lstrcmpW(about, was)) { /* saved even when empty, so an otherwise empty profile still exists */
        WritePrivateProfileStringW(old, L"description", about, ini);
        SetDlgItemTextW(d, IDC_EDIT_STATUS, L"Saved.");
    }
    GetDlgItemTextW(d, IDC_NAME, name, 64);
    trim(name);
    if (!lstrcmpW(name, old)) {
        if (group_changed) editor_fill(d, old), SetDlgItemTextW(d, IDC_EDIT_STATUS, L"Saved.");
        return;
    }
    const wchar_t *why = bad_name(name, old);
    if (why || !rename_section(old, name)) {
        wsprintfW(msg, L"Not renamed. %s", why ? why : L"Arges couldn't write its settings file.");
        SetDlgItemTextW(d, IDC_NAME, old);
    } else {
        GetPrivateProfileStringW(L"general", L"profile", L"", was, 64, ini);
        if (!lstrcmpiW(was, old)) WritePrivateProfileStringW(L"general", L"profile", name, ini);
        retarget(old, name);
        editor_fill(d, name);
        wsprintfW(msg, L"Renamed %s to %s.", old, name);
    }
    SetDlgItemTextW(d, IDC_EDIT_STATUS, msg);
}

static void editor_add(HWND d)
{
    wchar_t name[64] = L"New profile", g[32], msg[160];
    editor_commit(d);
    for (int n = 2; bad_name(name, L""); n++) wsprintfW(name, L"New profile %d", n);
    GetPrivateProfileStringW(sections[edited], L"group", L"", g, 32, ini);
    WritePrivateProfileStringW(name, L"description", L"", ini);
    if (*g) WritePrivateProfileStringW(name, L"group", g, ini);
    editor_fill(d, name);
    SetFocus(GetDlgItem(d, IDC_NAME));
    SendDlgItemMessageW(d, IDC_NAME, EM_SETSEL, 0, -1);
    wsprintfW(msg, L"Added %s. It uses Default until you change it in the settings window.", name);
    SetDlgItemTextW(d, IDC_EDIT_STATUS, msg);
}

static void editor_delete(HWND d)
{
    wchar_t name[64], next[64], in_use[64], msg[200];
    if (section_count < 2) {
        SetDlgItemTextW(d, IDC_EDIT_STATUS, L"Not deleted. Arges needs at least one profile.");
        return;
    }
    lstrcpynW(name, sections[edited], 64);
    lstrcpynW(next, sections[edited ? edited - 1 : 1], 64);
    wsprintfW(msg, L"Delete %s? Its settings are lost, and schedule stretches that switch to it are removed.", name);
    if (MessageBoxW(d, msg, L"Delete profile", MB_OKCANCEL | MB_ICONWARNING) != IDOK) return;
    WritePrivateProfileStringW(name, NULL, NULL, ini);
    retarget(name, NULL);
    GetPrivateProfileStringW(L"general", L"profile", L"", in_use, 64, ini);
    if (!lstrcmpiW(in_use, name)) WritePrivateProfileStringW(L"general", L"profile", next, ini);
    editor_fill(d, next);
    wsprintfW(msg, L"Deleted %s.", name);
    SetDlgItemTextW(d, IDC_EDIT_STATUS, msg);
}

static INT_PTR CALLBACK editor_proc(HWND d, UINT msg, WPARAM wp, LPARAM lp)
{
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG: {
        HICON icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
        SendMessageW(d, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        add(d, L"ListBox", L"", LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP, IDC_LIST, 8, 8, 110, 130);
        add(d, L"Static", L"Name", 0, -1, 126, 10, 44, 10);
        add(d, L"Edit", L"", ES_AUTOHSCROLL | WS_TABSTOP, IDC_NAME, 172, 8, 120, 13);
        add(d, L"Static", L"Group", 0, -1, 126, 28, 44, 10);
        add(d, L"ComboBox", L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, IDC_GROUP, 172, 26, 120, 120);
        add(d, L"Static", L"Description", 0, -1, 126, 46, 44, 10);
        add(d, L"Edit", L"", ES_AUTOHSCROLL | WS_TABSTOP, IDC_ABOUT, 172, 44, 120, 13);
        add(d, L"Static", L"Changes save when you leave a box. Leave Group empty to put the profile on the tray "
            L"menu itself rather than in a group.", 0, IDC_HINT, 126, 64, 166, 28);
        add(d, L"Static", L"", 0, IDC_EDIT_STATUS, 126, 98, 166, 28);
        add(d, L"Button", L"Add", BS_PUSHBUTTON | WS_TABSTOP, IDC_ADD, 8, 144, 52, 14);
        add(d, L"Button", L"Delete", BS_PUSHBUTTON | WS_TABSTOP, IDC_DELETE, 66, 144, 52, 14);
        add(d, L"Button", L"Close", BS_PUSHBUTTON | WS_TABSTOP, IDCANCEL, 240, 144, 52, 14);
        SendDlgItemMessageW(d, IDC_NAME, EM_LIMITTEXT, 63, 0);
        SendDlgItemMessageW(d, IDC_GROUP, CB_LIMITTEXT, 31, 0);
        SendDlgItemMessageW(d, IDC_ABOUT, EM_LIMITTEXT, 255, 0);
        editor_fill(d, profile);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (id == IDC_LIST && code == LBN_SELCHANGE) {
            edited = (int)SendDlgItemMessageW(d, IDC_LIST, LB_GETCURSEL, 0, 0);
            editor_show(d);
        } else if (((id == IDC_NAME || id == IDC_ABOUT) && code == EN_KILLFOCUS) || (id == IDC_GROUP && code == CBN_KILLFOCUS)
                   || id == IDOK) {
            editor_commit(d);
        } else if (id == IDC_ADD && code == BN_CLICKED) {
            editor_add(d);
        } else if (id == IDC_DELETE && code == BN_CLICKED) {
            editor_delete(d);
        } else if (id == IDCANCEL) {
            editor_commit(d);
            EndDialog(d, 0);
        }
        return TRUE;
    }
    case WM_CTLCOLORSTATIC:
        if (GetDlgCtrlID((HWND)lp) != IDC_HINT) return FALSE;
        SetTextColor((HDC)wp, GetSysColor(COLOR_GRAYTEXT));
        SetBkMode((HDC)wp, TRANSPARENT);
        return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
    }
    return FALSE;
}

static void edit_profiles(HWND w)
{
    HWND p = GetDlgItem(w, IDC_PROFILE), target = GetDlgItem(w, IDC_TARGET);
    wchar_t s[512];
    DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_PROFILES), w, editor_proc, 0);
    load_sections();
    GetPrivateProfileStringW(L"general", L"profile", L"", profile, 64, ini);
    int k = 0;
    while (k < section_count && lstrcmpiW(sections[k], profile)) k++;
    if (k == section_count) lstrcpynW(profile, sections[k = 0], 64);
    loading = 1;
    SendMessageW(p, CB_RESETCONTENT, 0, 0);
    fill_profile_list(p, 0);
    SendMessageW(p, CB_SETCURSEL, profile_item = item_of(p, defaults ? DEFAULTS_ITEM : k), 0);
    SendMessageW(target, CB_RESETCONTENT, 0, 0);
    fill_profile_list(target, 1);
    loading = 0;
    GetPrivateProfileStringW(L"general", L"schedule", L"", s, 512, ini);
    sched_count = parse_schedule(s, sched, MAX_SLOTS);
    if (picked >= sched_count) picked = sched_count - 1;
    show_picked(w);
    refresh(w);
    describe(w);
}

static void profile_picked(HWND w)
{
    HWND p = GetDlgItem(w, IDC_PROFILE);
    wchar_t s[128];
    int k = (int)SendMessageW(p, CB_GETITEMDATA, profile_item = pick_item(p, profile_item), 0);
    defaults = k == DEFAULTS_ITEM;
    if (!defaults) {
        lstrcpynW(profile, sections[k], 64);
        WritePrivateProfileStringW(L"general", L"profile", profile, ini);
        WritePrivateProfileStringW(L"general", L"random", NULL, ini);
        wsprintfW(s, L"Now using %s. Type on the keyboard to see it.", profile);
    } else lstrcpyW(s, L"Editing Default.");
    refresh(w);
    describe(w);
    SetDlgItemTextW(w, IDC_STATUS, s);
}

static INT_PTR CALLBACK dlgproc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowTextW(w, SETTINGS_TITLE);
        HICON icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
        SendMessageW(w, WM_SETICON, ICON_BIG, (LPARAM)icon);
        SendMessageW(w, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        build(w);
        refresh(w);
        describe(w);
        pv.s.last_tick = pv.s.heat_time = (double)GetTickCount64();
        SetTimer(w, TIMER_KEYBOARD, 33, NULL);
        return TRUE;
    }
    case WM_TIMER:
        if (wp == TIMER_KEYBOARD && !IsIconic(w)) {
            preview_step();
            InvalidateRect(GetDlgItem(w, IDC_KEYBOARD), NULL, FALSE);
        }
        return TRUE;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (loading) return TRUE;
        if (id == IDC_PROFILE && code == CBN_SELCHANGE) profile_picked(w);
        else if (id == IDC_TARGET && code == CBN_SELCHANGE) target_picked(w);
        else if (id == IDC_REMOVE && code == BN_CLICKED) remove_picked(w);
        else if (id == IDC_RESTORE && code == BN_CLICKED) restore(w);
        else if (id == IDC_EDIT && code == BN_CLICKED) edit_profiles(w);
        else if (id >= IDC_FIELD && id < IDC_FIELD + FIELD_COUNT && (code == BN_CLICKED || code == CBN_SELCHANGE))
            changed(w, id - IDC_FIELD, 0);
        else if (id >= IDC_STEP && id < IDC_STEP + FIELD_COUNT * SLOTS && code == BN_CLICKED)
            changed(w, (id - IDC_STEP) / SLOTS, (id - IDC_STEP) % SLOTS);
        else if (id >= IDC_SWATCH && id < IDC_SWATCH + FIELD_COUNT * SLOTS && code == BN_CLICKED)
            pick(w, (id - IDC_SWATCH) / SLOTS, (id - IDC_SWATCH) % SLOTS);
        else if (id == IDCANCEL) EndDialog(w, 0);
        return TRUE;
    }
    case WM_HSCROLL: {
        int f = GetDlgCtrlID((HWND)lp) - IDC_FIELD;
        if (f < 0 || f >= FIELD_COUNT || loading) return TRUE;
        show_value(w, f, slider_value(f, (int)SendMessageW((HWND)lp, TBM_GETPOS, 0, 0)));
        if (LOWORD(wp) != TB_THUMBTRACK) changed(w, f, 0);
        return TRUE;
    }
    case WM_CONTEXTMENU: {
        int id = GetDlgCtrlID((HWND)wp) - IDC_SWATCH;
        if (id >= 0 && id < FIELD_COUNT * SLOTS && IsWindowEnabled((HWND)wp)) unpick(w, id / SLOTS, id % SLOTS);
        return TRUE;
    }
    case WM_NOTIFY: {
        NMTTDISPINFOW *t = (NMTTDISPINFOW *)lp;
        if (t->hdr.code == TTN_GETDISPINFOW) t->lpszText = (wchar_t *)tip_for(GetDlgCtrlID((HWND)t->hdr.idFrom));
        return TRUE;
    }
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *m = (MEASUREITEMSTRUCT *)lp;
        RECT r = {0, 0, 0, m->itemID == (UINT)-1 ? 10 : 9};
        MapDialogRect(w, &r);
        m->itemHeight = r.bottom;
        return TRUE;
    }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT *d = (const DRAWITEMSTRUCT *)lp;
        if (d->CtlType == ODT_COMBOBOX) draw_profile_item(d);
        else if (d->CtlID == IDC_FADE) draw_fade(d);
        else if (d->CtlID == IDC_KEYBOARD) draw_keyboard(d);
        else draw_swatch(d);
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        int id = GetDlgCtrlID((HWND)lp);
        if (id != IDC_STATIC_HELP && id != IDC_REACTIVE_HELP && id != IDC_SCHEDULE_HELP) return FALSE;
        SetTextColor((HDC)wp, GetSysColor(COLOR_GRAYTEXT));
        SetBkMode((HDC)wp, TRANSPARENT);
        return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
    }
    }
    return FALSE;
}

/* arges.exe --settings [ini] runs this instead of the tray app */
int settings_main(HINSTANCE inst, const wchar_t *ini_path)
{
    HWND open = FindWindowW(L"#32770", SETTINGS_TITLE);
    if (open) {
        ShowWindow(open, SW_RESTORE);
        SetForegroundWindow(open);
        return 0;
    }

    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    ACTCTXW ac = {sizeof ac};
    ac.dwFlags = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID;
    ac.lpSource = exe;
    ac.hModule = inst;
    ac.lpResourceName = MAKEINTRESOURCEW(2);
    ULONG_PTR cookie;
    ActivateActCtx(CreateActCtxW(&ac), &cookie);
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_TAB_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);
    SetProcessDPIAware();
    WNDCLASSW day = {0};
    day.lpfnWndProc = day_proc;
    day.hInstance = inst;
    day.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    day.lpszClassName = L"ArgesDay";
    RegisterClassW(&day);

    if (ini_path) lstrcpynW(ini, ini_path, MAX_PATH);
    else {
        lstrcpyW(ini, exe);
        lstrcpyW(wcsrchr(ini, L'\\') + 1, L"arges.ini");
    }

    load_sections();
    if (!section_count) {
        wchar_t msg[MAX_PATH + 128];
        wsprintfW(msg, L"Couldn't find any profiles in %s.\n\nRun arges.exe once to create them, then open settings again.", ini);
        MessageBoxW(NULL, msg, L"Arges RGB settings", MB_ICONWARNING);
        return 1;
    }
    GetPrivateProfileStringW(L"general", L"profile", L"Poseidon", profile, 64, ini); /* same fallback as arges.c */
    DialogBoxParamW(inst, MAKEINTRESOURCEW(IDD_SETTINGS), NULL, dlgproc, 0);
    return 0;
}
