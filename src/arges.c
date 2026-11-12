#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <wtsapi32.h>
#include <stdio.h>
#include <stdlib.h>
#include "effects.h"
#include "supported_keyboards.h"

enum { WM_TRAY = WM_APP + 1, ID_PAUSE = 1, ID_STARTUP, ID_EDIT, ID_QUIT, ID_RANDOM, ID_DIM, ID_PROFILE = 100 };
enum { TIMER_FRAME = 1, TIMER_HOUR, TIMER_IDLE_OFF, TIMER_RETRY };
static const GUID DISPLAY_STATE = {0x6fe69556, 0x704a, 0x47a0, {0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47}}; /* GUID_CONSOLE_DISPLAY_STATE */
#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

static wchar_t ini[MAX_PATH], profile[64], scheduled[64], *active = profile;
static Slot slots[MAX_SLOTS];
static int slot_count, slot_on = -1, random_slot = -1;
static FILETIME ini_time;
static Lighting lighting;
static LightState light = {.ss = {1}};
static int fps, dim_level, dimmed, dim_on, scheduled_off, random_on;
static int off_after, off_with_screen;

static HANDLE dev = INVALID_HANDLE_VALUE;
static const Keyboard *kb;
static ULONGLONG last_input, started;
static unsigned char shown[3];
static int shown_valid, timer_on, paused, locked, screen_off;

static NOTIFYICONDATAW nid;
static HICON icon_on, icon_paused;
static UINT taskbar_created;

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void update_tray(DWORD op)
{
    nid.hIcon = paused ? icon_paused : icon_on;
    wsprintfW(nid.szTip, paused ? L"Arges RGB: paused" : active != profile ? L"Arges RGB: %s (scheduled)"
                         : random_on ? L"Arges RGB: %s (random)" : L"Arges RGB: %s", active);
    Shell_NotifyIconW(op, &nid);
}

/* The section's own value, then [general]'s, then def. */
static int read_int(const wchar_t *path, const wchar_t *section, const wchar_t *key, int def, int lo, int hi)
{
    def = GetPrivateProfileIntW(L"general", key, def, path);
    return clamp(GetPrivateProfileIntW(section, key, def, path), lo, hi);
}

static int read_colours(const wchar_t *path, const wchar_t *section, const wchar_t *key, const wchar_t *def, int c[][3],
                        int max, int may_be_empty)
{
    wchar_t general[256], s[256];
    GetPrivateProfileStringW(L"general", key, def, general, 256, path);
    GetPrivateProfileStringW(section, key, general, s, 256, path);
    int n = parse_colours(s, c, max);
    if (!n && (*s || !may_be_empty)) n = parse_colours(def, c, max);
    for (int k = 0; k < n; k++)
        for (int i = 0; i < 3; i++) c[k][i] = clamp(c[k][i], 0, 255);
    return n;
}

/* Also used by the settings window, so these two take the ini path. */
int static_in(const wchar_t *path, const wchar_t *section, Static *s)
{
    wchar_t name[32];
    GetPrivateProfileStringW(section, L"static_effect", L"", name, 32, path);
    return static_of(name_index(name, STATIC_NAMES, STATIC_EFFECTS), GetPrivateProfileIntW(section, L"static_speed", -1, path),
                     GetPrivateProfileIntW(section, L"static_strength", -1, path), s);
}

/* The settings window's FIELDS builtin values must match the defaults here. */
void load_lighting(const wchar_t *path, const wchar_t *section, Lighting *l)
{
    l->cols_n    = read_colours(path, section, L"reactive_colour", L"0,255,180", l->cols, MAX_COLOURS, 0);
    l->glow_n    = read_colours(path, section, L"reactive_afterglow", L"", l->glow, 3, 1);
    l->still_n   = read_colours(path, section, L"static_colour", L"0,0,0", l->still, MAX_COLOURS, 0);
    l->seasons_n = read_colours(path, section, L"static_seasons", L"", l->seasons, 4, 1);
    l->static_shuffle   = read_int(path, section, L"static_shuffle", 0, 0, 1);
    l->reactive_shuffle = read_int(path, section, L"reactive_shuffle", 0, 0, 1);
    l->fx.out_ms = read_int(path, section, L"reactive_duration", 100, 1, 10000);
    l->fx.in_ms  = read_int(path, section, L"reactive_fade_in", 0, 0, 10000);
    l->fx.curve  = read_int(path, section, L"reactive_curve", 1, 1, 5);
    l->fx.echoes = read_int(path, section, L"reactive_echo", 0, 0, 4);
    l->fx.echo   = read_int(path, section, L"reactive_echo_strength", 50, 1, 100) / 100.0;
    l->hold      = read_int(path, section, L"reactive_hold", 1, 0, 1);
    if (!static_in(path, section, &l->st) && !static_in(path, L"general", &l->st)) l->st = (Static){STILL, 5, 0};
    wchar_t general_effect[32], name[32];
    GetPrivateProfileStringW(L"general", L"reactive_effect", L"", general_effect, 32, path);
    GetPrivateProfileStringW(section, L"reactive_effect", general_effect, name, 32, path);
    l->effect = reactive_index(name);
    l->static_bright   = read_int(path, section, L"static_brightness", 100, 0, 100);
    l->reactive_bright = read_int(path, section, L"reactive_brightness", 100, 0, 100);
}

static int slot_now(void)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    return slot_at(slots, slot_count, t.wHour);
}

static void pick_random(wchar_t out[64], const wchar_t *not)
{
    wchar_t names[8192], *list[200];
    int n = 0;
    GetPrivateProfileSectionNamesW(names, 8192, ini);
    for (wchar_t *s = names; *s && n < 200; s += lstrlenW(s) + 1)
        if (lstrcmpiW(s, L"general") && lstrcmpiW(s, not)) list[n++] = s;
    lstrcpynW(out, n ? list[rand() % n] : not, 64);
}

static int has_profile(const wchar_t *name)
{
    wchar_t keys[64];
    return GetPrivateProfileStringW(name, NULL, L"", keys, 64, ini) > 0;
}

static void load_settings(void)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(ini, GetFileExInfoStandard, &a)) {
        HANDLE f = CreateFileW(ini, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(3), (LPCWSTR)RT_RCDATA);
            DWORD w;
            if (r) WriteFile(f, LockResource(LoadResource(NULL, r)), SizeofResource(NULL, r), &w, NULL);
            CloseHandle(f);
        }
        if (!GetFileAttributesExW(ini, GetFileExInfoStandard, &a)) a.ftLastWriteTime = (FILETIME){1, 0};
    }
    if (shown_valid && CompareFileTime(&a.ftLastWriteTime, &ini_time) == 0 && slot_now() == slot_on)
        return;
    ini_time = a.ftLastWriteTime;

    GetPrivateProfileStringW(L"general", L"profile", L"Poseidon", profile, 64, ini);
    random_on = GetPrivateProfileIntW(L"general", L"random", 0, ini) != 0;
    wchar_t sched[512];
    GetPrivateProfileStringW(L"general", L"schedule", L"", sched, 512, ini);
    slot_count = parse_schedule(sched, slots, MAX_SLOTS);
    slot_on = slot_now();
    active = profile;
    dimmed = scheduled_off = 0;
    if (slot_on < 0) random_slot = -1;
    else if (!lstrcmpiW(slots[slot_on].target, L"dim")) dimmed = 1, random_slot = -1;
    else if (!lstrcmpiW(slots[slot_on].target, L"off")) scheduled_off = 1, random_slot = -1;
    else if (!lstrcmpiW(slots[slot_on].target, L"random")) {
        if (random_slot != slot_on || !has_profile(scheduled)) pick_random(scheduled, profile);
        random_slot = slot_on;
        active = scheduled;
    } else {
        random_slot = -1;
        lstrcpynW(scheduled, slots[slot_on].target, 64);
        if (has_profile(scheduled)) active = scheduled;
    }

    load_lighting(ini, active, &lighting);
    fps         = read_int(ini, active, L"fps", 45, 1, 100);
    dim_level   = read_int(ini, active, L"dim", 20, 0, 100);
    off_after   = clamp(GetPrivateProfileIntW(L"general", L"off_after", 0, ini), 0, 1440);
    off_with_screen = GetPrivateProfileIntW(L"general", L"off_with_screen", 1, ini) != 0;
    update_tray(NIM_MODIFY);
}

static const Keyboard *match_keyboard(wchar_t *path, int with_iface)
{
    CharLowerW(path);
    for (int i = 0; i < KEYBOARD_COUNT; i++) {
        wchar_t id[32];
        wsprintfW(id, with_iface ? L"vid_%04x&pid_%04x&mi_%02x" : L"vid_%04x&pid_%04x",
                  KEYBOARDS[i].vid, KEYBOARDS[i].pid, KEYBOARDS[i].iface);
        if (wcsstr(path, id)) return &KEYBOARDS[i];
    }
    return NULL;
}

static HANDLE open_keyboard(void)
{
    GUID g;
    HidD_GetHidGuid(&g);
    HDEVINFO set = SetupDiGetClassDevsW(&g, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    HANDLE h = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA ifd = {sizeof ifd};

    for (DWORD i = 0; h == INVALID_HANDLE_VALUE && SetupDiEnumDeviceInterfaces(set, NULL, &g, i, &ifd); i++) {
        union { SP_DEVICE_INTERFACE_DETAIL_DATA_W d; char buf[2048]; } u;
        u.d.cbSize = sizeof u.d;
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifd, &u.d, sizeof u, NULL, NULL)) continue;

        wchar_t lower[512];
        lstrcpynW(lower, u.d.DevicePath, 512);
        const Keyboard *found = match_keyboard(lower, 1);
        if (!found) continue;

        HANDLE c = CreateFileW(u.d.DevicePath, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        /* Windows locks keyboard interfaces, but feature reports work without read/write access */
        if (c == INVALID_HANDLE_VALUE)
            c = CreateFileW(u.d.DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (c == INVALID_HANDLE_VALUE) continue;

        PHIDP_PREPARSED_DATA pp;
        HIDP_CAPS caps = {0};
        if (HidD_GetPreparsedData(c, &pp)) {
            HidP_GetCaps(pp, &caps);
            HidD_FreePreparsedData(pp);
        }
        if (caps.UsagePage == 1 && caps.Usage == 2) h = c, kb = found;
        else CloseHandle(c);
    }
    SetupDiDestroyDeviceInfoList(set);
    return h;
}

/* After a failed send, look for the keyboard at most every 2 s: the search is slow. */
static int send_colour(const unsigned char c[3])
{
    static ULONGLONG next_look;
    for (int tries = 0; tries < 2; tries++) {
        if (dev == INVALID_HANDLE_VALUE) {
            if (GetTickCount64() < next_look) return 0;
            dev = open_keyboard();
        }
        if (dev == INVALID_HANDLE_VALUE) break;

        unsigned char p[91];
        razer_set_colour(p, kb, c);
        BOOL ok = HidD_SetFeature(dev, p, 91);
        razer_show(p, kb);
        ok = ok && HidD_SetFeature(dev, p, 91);
        /* shortcut: after replugging, a static colour returns only on the next key press; watch WM_DEVICECHANGE to fix */
        if (ok) return 1;
        CloseHandle(dev);
        dev = INVALID_HANDLE_VALUE;
    }
    next_look = GetTickCount64() + 2000;
    return 0;
}

static void tick(HWND w)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    ULONGLONG now = GetTickCount64();
    int idle_off = off_after && now - last_input >= off_after * 60000ULL;
    int off = paused || locked || (screen_off && off_with_screen) || idle_off || scheduled_off;
    double bright = off ? 0 : dimmed || dim_on ? dim_level / 100.0 : 1.0;
    unsigned char c[3];
    int running = light_frame(&lighting, &light, (double)now, t.wMonth, bright, c) && !off;
    if (!shown_valid || memcmp(c, shown, 3)) {
        memcpy(shown, c, 3);
        shown_valid = send_colour(c);
    }
    /* the keyboard may not be ready yet at sign-in */
    if (!shown_valid && now - started < 60000) SetTimer(w, TIMER_RETRY, 2000, NULL);
    else KillTimer(w, TIMER_RETRY);
    running |= !off && light_moving(&lighting);
    if (running && !timer_on) timer_on = (int)SetTimer(w, TIMER_FRAME, 1000 / fps, NULL);
    if (!running && timer_on) { KillTimer(w, TIMER_FRAME); timer_on = 0; }
    SetTimer(w, TIMER_HOUR, (60 - t.wMinute) * 60000 - t.wSecond * 1000 - t.wMilliseconds, NULL);
    if (off || !off_after) KillTimer(w, TIMER_IDLE_OFF);
    else SetTimer(w, TIMER_IDLE_OFF, (UINT)(last_input + off_after * 60000ULL - now) + 50, NULL);
}

static int is_supported(HANDLE hdev)
{
    static HANDLE known[16];
    static char verdict[16];
    static int n;
    for (int i = 0; i < n && i < 16; i++)
        if (known[i] == hdev) return verdict[i];

    wchar_t name[512];
    UINT len = 512;
    int yes = 0;
    if ((int)GetRawInputDeviceInfoW(hdev, RIDI_DEVICENAME, name, &len) > 0)
        yes = match_keyboard(name, 0) != NULL;
    known[n % 16] = hdev;
    verdict[n % 16] = (char)yes;
    n++;
    return yes;
}

static HICON load_icon(HINSTANCE inst, int id)
{
    return LoadImageW(inst, MAKEINTRESOURCEW(id), IMAGE_ICON,
                      GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
}

static int starts_with_windows(const wchar_t *run)
{
    wchar_t v[MAX_PATH + 3];
    DWORD sz = sizeof v;
    return RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, L"Arges RGB", RRF_RT_REG_SZ, NULL, v, &sz) == ERROR_SUCCESS
        && !lstrcmpiW(v, run);
}

static void random_profile(void)
{
    wchar_t pick[64];
    pick_random(pick, profile);
    WritePrivateProfileStringW(L"general", L"profile", pick, ini);
    load_settings();
}

static void open_settings(void)
{
    wchar_t exe[MAX_PATH], args[MAX_PATH + 16];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wsprintfW(args, L"--settings \"%s\"", ini);
    ShellExecuteW(NULL, NULL, exe, args, NULL, SW_SHOWNORMAL);
}

static void show_menu(HWND w)
{
    enum { MAX_GROUPS = 16, MAX_PROFILES = 200 };
    HMENU m = CreatePopupMenu(), group_menu[MAX_GROUPS];
    int group_pos[MAX_GROUPS];
    wchar_t names[8192], group_name[MAX_GROUPS][32], *profile_of[MAX_PROFILES];
    int groups = 0, profiles = 0;
    GetPrivateProfileSectionNamesW(names, 8192, ini);
    for (wchar_t *s = names; *s && profiles < MAX_PROFILES; s += lstrlenW(s) + 1) {
        if (!lstrcmpiW(s, L"general")) continue;
        wchar_t g[32];
        int i = 0, current = !lstrcmpiW(s, profile);
        GetPrivateProfileStringW(s, L"group", L"", g, 32, ini);
        while (i < groups && lstrcmpiW(g, group_name[i])) i++;
        if (*g && i == groups && groups < MAX_GROUPS) {
            group_menu[groups] = CreatePopupMenu();
            group_pos[groups] = GetMenuItemCount(m);
            lstrcpyW(group_name[groups++], g);
            AppendMenuW(m, MF_POPUP, (UINT_PTR)group_menu[i], g);
        }
        HMENU parent = *g && i < groups ? group_menu[i] : m;
        current = current && !random_on;
        if (current && parent != m)
            CheckMenuItem(m, group_pos[i], MF_BYPOSITION | MF_CHECKED);
        AppendMenuW(parent, MF_STRING | (current ? MF_CHECKED : 0), ID_PROFILE + profiles, s);
        profile_of[profiles++] = s;
    }
    AppendMenuW(m, MF_STRING | (random_on ? MF_CHECKED : 0), ID_RANDOM, L"Random profile");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_EDIT, L"Edit settings");
    AppendMenuW(m, MF_STRING | (dim_on ? MF_CHECKED : 0), ID_DIM, L"Dim");
    AppendMenuW(m, MF_STRING | (paused ? MF_CHECKED : 0), ID_PAUSE, L"Pause");
    wchar_t exe_path[MAX_PATH], run[MAX_PATH + 3];
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    wsprintfW(run, L"\"%s\"", exe_path);
    int startup = starts_with_windows(run);
    AppendMenuW(m, MF_STRING | (startup ? MF_CHECKED : 0), ID_STARTUP, L"Start with Windows");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_QUIT, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(w); /* or the menu won't close on a click elsewhere */
    UINT cmd = (UINT)TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, w, NULL);
    PostMessageW(w, WM_NULL, 0, 0);

    if (cmd >= ID_PROFILE && cmd < (UINT)(ID_PROFILE + profiles)) {
        WritePrivateProfileStringW(L"general", L"profile", profile_of[cmd - ID_PROFILE], ini);
        WritePrivateProfileStringW(L"general", L"random", NULL, ini);
        load_settings();
        tick(w);
    } else if (cmd == ID_RANDOM) {
        WritePrivateProfileStringW(L"general", L"random", random_on ? NULL : L"1", ini);
        if (random_on) load_settings();
        else random_profile();
        tick(w);
    } else if (cmd == ID_DIM) {
        dim_on = !dim_on;
        tick(w);
    } else if (cmd == ID_PAUSE) {
        paused = !paused;
        light.held = 0;
        update_tray(NIM_MODIFY);
        tick(w);
    } else if (cmd == ID_STARTUP) {
        if (startup) RegDeleteKeyValueW(HKEY_CURRENT_USER, RUN_KEY, L"Arges RGB");
        else RegSetKeyValueW(HKEY_CURRENT_USER, RUN_KEY, L"Arges RGB", REG_SZ, run, (lstrlenW(run) + 1) * sizeof *run);
    } else if (cmd == ID_EDIT) {
        open_settings();
    } else if (cmd == ID_QUIT) {
        PostMessageW(w, WM_CLOSE, 0, 0);
    }
    DestroyMenu(m);
}

static LRESULT CALLBACK wndproc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == taskbar_created) update_tray(NIM_ADD); /* Explorer restarted */

    switch (msg) {
    case WM_INPUT: {
        RAWINPUT ri;
        UINT sz = sizeof ri;
        /* break, not return: DefWindowProc must see WM_INPUT too */
        if (paused || locked
            || GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &sz, sizeof(RAWINPUTHEADER)) == (UINT)-1
            || ri.header.dwType != RIM_TYPEKEYBOARD || !is_supported(ri.header.hDevice))
            break;
        if (ri.data.keyboard.Flags & RI_KEY_BREAK) {
            if (light_release(&lighting, &light, (double)GetTickCount64())) tick(w);
            break;
        }
        last_input = GetTickCount64();
        if (!light.held) load_settings(); /* not on auto-repeat */
        light_press(&lighting, &light, (double)last_input);
        tick(w);
        break;
    }
    case WM_WTSSESSION_CHANGE:
        if (wp == WTS_SESSION_LOCK || wp == WTS_SESSION_UNLOCK) {
            locked = wp == WTS_SESSION_LOCK;
            light.held = 0; /* the key-up may never arrive */
            tick(w);
        }
        return 0;
    case WM_TIMER:
        if (wp == TIMER_HOUR) {
            load_settings();
            if (random_on) random_profile();
        }
        if (wp == TIMER_IDLE_OFF) KillTimer(w, TIMER_IDLE_OFF);
        tick(w);
        return 0;
    case WM_POWERBROADCAST:
        if (wp == PBT_POWERSETTINGCHANGE) {
            const POWERBROADCAST_SETTING *ps = (const POWERBROADCAST_SETTING *)lp;
            if (IsEqualGUID(&ps->PowerSetting, &DISPLAY_STATE)) screen_off = *(const DWORD *)ps->Data == 0;
        }
        if (wp == PBT_POWERSETTINGCHANGE || wp == PBT_APMRESUMEAUTOMATIC) {
            shown_valid = 0;
            load_settings();
            tick(w);
        }
        return TRUE;
    case WM_TRAY:
        if (lp == WM_RBUTTONUP) show_menu(w);
        if (lp == WM_LBUTTONDBLCLK) open_settings();
        return 0;
    case WM_CLOSE: {
        unsigned char off[3] = {0, 0, 0};
        send_colour(off);
        DestroyWindow(w);
        return 0;
    }
    case WM_ENDSESSION: { /* sign-out and shutdown skip WM_CLOSE */
        unsigned char off[3] = {0, 0, 0};
        if (wp) send_colour(off);
        return 0;
    }
    case WM_DESTROY:
        WTSUnRegisterSessionNotification(w);
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int settings_main(HINSTANCE inst, const wchar_t *ini_path); /* settings.c */

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    (void)prev; (void)cmd; (void)show;
    const wchar_t *cls = L"ArgesRGB";
    if (__argc > 1 && !lstrcmpiW(__wargv[1], L"--settings")) return settings_main(inst, __argc > 2 ? __wargv[2] : NULL);

    CreateMutexW(NULL, FALSE, L"ArgesRGBRunning");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    SetProcessDPIAware();
    srand(GetTickCount());
    GetModuleFileNameW(NULL, ini, MAX_PATH);
    wcscpy(wcsrchr(ini, L'.'), L".ini");
    load_settings();
    if (random_on) random_profile();
    taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = cls;
    RegisterClassW(&wc);
    /* not message-only: the tray menu needs a window that can take the foreground */
    HWND w = CreateWindowExW(0, cls, L"Arges RGB", 0, 0, 0, 0, 0, NULL, NULL, inst, NULL);

    RAWINPUTDEVICE rid = {1, 6, RIDEV_INPUTSINK, w};
    if (!w || !RegisterRawInputDevices(&rid, 1, sizeof rid)) {
        MessageBoxW(NULL, L"Couldn't start listening for key presses.", L"Arges RGB", MB_ICONERROR);
        return 1;
    }
    WTSRegisterSessionNotification(w, NOTIFY_FOR_THIS_SESSION);
    RegisterPowerSettingNotification(w, &DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

    icon_on = load_icon(inst, 1);
    icon_paused = load_icon(inst, 2);
    nid.cbSize = sizeof nid;
    nid.hWnd = w;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    update_tray(NIM_ADD);

    dev = open_keyboard();
    started = last_input = GetTickCount64();
    light.last_tick = light.heat_time = (double)started;
    tick(w);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) DispatchMessageW(&m);
    return 0;
}
