// 3ds-fastfetch — a fastfetch-style system info tool for the Nintendo 3DS.
// Copyright (C) 2026 xPsycho999
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. This program is distributed WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License (LICENSE file) for more details.
//
// Design rule: ONLY display information actually read from the console.
// Every value comes from a libctru/OS call that is checked; if a call fails
// (e.g. a service that doesn't exist under an emulator), that field is simply
// omitted rather than shown with a fabricated placeholder.
//
// Inspiration: joel16/3DSident (the system-info APIs) and aliceinpalth/3dfetch
// (the fastfetch-style logo + columns layout).
//
// Build:  make            -> 3ds-fastfetch.3dsx
// Run:    Homebrew Launcher on CFW (real values), or Azahar/Citra (subset).

#include <3ds.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

// ---- ANSI colors (the console understands VT codes) ----
#define RESET  "\x1b[0m"
#define RED    "\x1b[31;1m"
#define GREEN  "\x1b[32;1m"
#define YELLOW "\x1b[33;1m"
#define BLUE   "\x1b[34;1m"
#define CYAN   "\x1b[36;1m"
#define WHITE  "\x1b[37;1m"

// ---- info rows, built dynamically so failed fields can be skipped ----
#define MAX_ROWS 28
static char rows[MAX_ROWS][96];
static int  row_count = 0;

static void add_row(const char *label, const char *fmt, ...) {
    if (row_count >= MAX_ROWS) return;
    char value[72];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(value, sizeof(value), fmt, ap);
    va_end(ap);
    snprintf(rows[row_count], sizeof(rows[row_count]),
             YELLOW "%-9s" RESET " %s", label, value);
    row_count++;
}

static void add_raw(const char *fmt, ...) {
    if (row_count >= MAX_ROWS) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(rows[row_count], sizeof(rows[row_count]), fmt, ap);
    va_end(ap);
    row_count++;
}

// ---- small ASCII clamshell-3DS logo (plain text; colored on print) ----
// Model-specific ASCII art. Every line must be <= LOGO_W chars; shorter lines
// are space-padded on print so the info column always starts at a fixed offset.
#define LOGO_W 14

// Old 3DS / Old 3DS XL: clamshell, circle pad + face buttons, no C-stick.
static const char *logo_old[] = {
    ".------------.",
    "| .--------. |",
    "| |        | |",
    "| '--------' |",
    "|------------|",
    "|(o).----.   |",
    "| + |    | * |",
    "|   '----'   |",
    "'------------'",
};
// New 3DS / New 3DS XL / New 2DS XL: adds the C-stick (o, upper right) and
// the second button cluster (*) — the features that distinguish the "New" line.
static const char *logo_new[] = {
    ".------------.",
    "| .--------. |",
    "| |        | |",
    "| '--------' |",
    "|------------|",
    "|(o).----. o |",
    "| + |    | * |",
    "|   '----'   |",
    "'------------'",
};
// 2DS: slate (wedge) form factor — both screens in a single non-folding slab.
static const char *logo_2ds[] = {
    ".------------.",
    "| .--------. |",
    "| |        | |",
    "| '--------' |",
    "|(o).----. * |",
    "| + |    |   |",
    "|   '----'   |",
    "'------------'",
};

// Pick the art that matches the detected model (CFG_MODEL_* values).
static const char **select_logo(int model, int *lines) {
    switch (model) {
        case 0: case 1:   // Old 3DS, Old 3DS XL
            *lines = sizeof(logo_old) / sizeof(logo_old[0]); return logo_old;
        case 3:           // 2DS
            *lines = sizeof(logo_2ds) / sizeof(logo_2ds[0]); return logo_2ds;
        default:          // New 3DS / New 3DS XL / New 2DS XL, or unknown model
            *lines = sizeof(logo_new) / sizeof(logo_new[0]); return logo_new;
    }
}

// ---- helpers -------------------------------------------------------------
static const char *model_name(u8 m) {
    switch (m) {
        case 0: return "Old 3DS (CTR)";
        case 1: return "Old 3DS XL (SPR)";
        case 2: return "New 3DS (KTR)";
        case 3: return "2DS (FTR)";
        case 4: return "New 3DS XL (RED)";
        case 5: return "New 2DS XL (JAN)";
        default: return NULL;
    }
}
static int model_is_new(u8 m) { return m == 2 || m == 4 || m == 5; }

static const char *region_name(u8 r) {
    static const char *n[] = {"Japan","USA","Europe","Australia","China","Korea","Taiwan"};
    return r < 7 ? n[r] : NULL;
}
static const char *language_name(u8 l) {
    static const char *n[] = {"Japanese","English","French","German","Italian","Spanish",
                              "Simp. Chinese","Korean","Dutch","Portuguese","Russian","Trad. Chinese"};
    return l < 12 ? n[l] : NULL;
}
static const char *vendor_panel(u8 nibble) {
    switch (nibble) {
        case 0x01: return "IPS";   // JDI
        case 0x0C: return "TN";    // SHARP
        default:   return NULL;
    }
}

// Add a storage row for a media type; omitted entirely if the query fails or
// the medium reports no space. Auto-scales between MiB and GiB.
static void add_storage(const char *label, FS_SystemMediaType mt) {
    FS_ArchiveResource res = {0};
    if (R_FAILED(FSUSER_GetArchiveResource(&res, mt)) || res.totalClusters == 0)
        return;
    u64 total = (u64)res.totalClusters * res.clusterSize;
    u64 freeb = (u64)res.freeClusters  * res.clusterSize;
    u64 used  = total - freeb;
    u64 pct   = total ? used * 100 / total : 0;
    if (total >= (1ULL << 30)) {
        double gib = (double)(1ULL << 30);
        add_row(label, "%.1f/%.1f GiB %llu%%", used / gib, total / gib, (unsigned long long)pct);
    } else {
        double mib = (double)(1ULL << 20);
        add_row(label, "%.0f/%.0f MiB %llu%%", used / mib, total / mib, (unsigned long long)pct);
    }
}

// Recompute the trailing check digit of a 3DS serial (same scheme as 3DSident),
// so the displayed serial matches the one printed on the console's sticker.
static int serial_check_digit(const u8 *s) {
    int odd = 0, even = 0, idx = 1;
    for (int i = 0; s[i]; i++) {
        if (s[i] >= '0' && s[i] <= '9') {
            int d = s[i] - '0';
            if (idx % 2 == 0) even += d; else odd += d;
            idx++;
        }
    }
    int c = ((3 * even) + odd) % 10;
    return c == 0 ? 0 : 10 - c;
}

int main(int argc, char **argv) {
    gfxInitDefault();
    // The libctru console renders to the single framebuffer it is bound to at
    // consoleInit and does NOT follow gfxSwapBuffers. With double-buffering only
    // one of the two buffers ever holds the text, so swapping makes the screen
    // alternate text/blank (flicker, and torn frames if redrawn). Using one
    // stable buffer keeps the screen correct and screenshots non-blank.
    gfxSetDoubleBuffering(GFX_TOP, false);
    consoleInit(GFX_TOP, NULL);

    // --- host/user title ---
    char username[32] = {0};
    int have_username = 0;
    u8 model = 0xFF;
    int have_model = 0;

    if (R_SUCCEEDED(cfguInit())) {
        // Username config block: u16 name[10] + u32 + u32  (0x1C bytes)
        struct { u16 name[10]; u32 zero; u32 ngword; } ub;
        if (R_SUCCEEDED(CFGU_GetConfigInfoBlk2(sizeof(ub), 0x000A0000, &ub))) {
            utf16_to_utf8((u8 *)username, ub.name, sizeof(username) - 1);
            if (username[0]) have_username = 1;
        }
        u8 m;
        if (R_SUCCEEDED(CFGU_GetSystemModel(&m))) { model = m; have_model = 1; }

        // Title row (fastfetch "user@host")
        if (have_username)
            add_raw(GREEN "%s" RESET "@" GREEN "3ds" RESET, username);
        else
            add_raw(GREEN "3ds-fastfetch" RESET);
        add_raw(WHITE "---------------------" RESET);

        if (have_model)
            add_row("Model", "%s", model_name(model) ? model_name(model) : "Unknown");

        u8 region;
        if (R_SUCCEEDED(CFGU_SecureInfoGetRegion(&region)) && region_name(region))
            add_row("Region", "%s", region_name(region));

        u8 lang;
        if (R_SUCCEEDED(CFGU_GetSystemLanguage(&lang)) && language_name(lang))
            add_row("Language", "%s", language_name(lang));

        // Serial number (needs the privileged cfg:i handle, available under CFW).
        // CFGI returns the serial without the trailing check digit, which we
        // recompute so it matches the number printed on the console.
        u8 serial[16] = {0};
        if (R_SUCCEEDED(CFGI_SecureInfoGetSerialNumber(serial)) && serial[0])
            add_row("Serial", "%s%d", (char *)serial, serial_check_digit(serial));

        cfguExit();
    }

    // --- OS / firmware version (NVer/CVer; mounts a system archive) ---
    if (R_SUCCEEDED(amInit())) {
        OS_VersionBin nver, cver;
        char sysver[32] = {0};
        if (R_SUCCEEDED(osGetSystemVersionDataString(&nver, &cver, sysver, sizeof(sysver))) && sysver[0])
            add_row("OS", "%s", sysver);
        amExit();
    }

    // --- Custom firmware (Luma3DS exposes its version via svcGetSystemInfo) ---
    {
        // Luma3DS: param 0 returns SYSTEM_VERSION(major,minor,build), packed
        // the same way as kernel/firmware versions (major<<24 | minor<<16 | build<<8).
        s64 out = 0;
        if (R_SUCCEEDED(svcGetSystemInfo(&out, 0x10000, 0)) && out != 0) {
            u32 v = (u32)out;
            add_row("CFW", "Luma3DS %lu.%lu.%lu",
                    (unsigned long)GET_VERSION_MAJOR(v),
                    (unsigned long)GET_VERSION_MINOR(v),
                    (unsigned long)GET_VERSION_REVISION(v));
        }
    }

    // --- Kernel ---
    {
        u32 k = osGetKernelVersion();
        add_row("Kernel", "%lu.%lu.%lu",
                (unsigned long)GET_VERSION_MAJOR(k),
                (unsigned long)GET_VERSION_MINOR(k),
                (unsigned long)GET_VERSION_REVISION(k));
    }

    // --- CPU / SoC (core count and max clock follow from the model) ---
    {
        bool is_new = false;
        if (have_model) is_new = model_is_new(model);
        else APT_CheckNew3DS(&is_new);
        // ARM11 MPCore: Old 3DS = 2 cores @ 268 MHz, New 3DS = 4 cores, up to 804 MHz.
        add_row("CPU", "ARM11 %s", is_new ? "4x @ 804 MHz" : "2x @ 268 MHz");
    }

    // --- Screen panels (New 3DS family only; old models are stubbed) ---
    if (have_model && model_is_new(model)) {
        if (R_SUCCEEDED(gspLcdInit())) {
            u8 v = 0;
            if (R_SUCCEEDED(GSPLCD_GetVendors(&v))) {
                const char *top = vendor_panel((v >> 4) & 0xF);
                const char *bot = vendor_panel(v & 0xF);
                if (top && bot)
                    add_row("Screens", "Top %s / Bottom %s", top, bot);
            }
            gspLcdExit();
        }
    }

    // --- Battery (exact % via MCU; charging state via PTM) ---
    {
        int have_pct = 0;
        u8 pct = 0;
        if (R_SUCCEEDED(mcuHwcInit())) {
            if (R_SUCCEEDED(MCUHWC_GetBatteryLevel(&pct))) have_pct = 1;
            mcuHwcExit();
        }
        u8 charging = 0;
        int have_charge = 0;
        if (R_SUCCEEDED(ptmuInit())) {
            if (R_SUCCEEDED(PTMU_GetBatteryChargeState(&charging))) have_charge = 1;
            if (!have_pct) {                    // fall back to coarse 0-5 level
                u8 lvl;
                if (R_SUCCEEDED(PTMU_GetBatteryLevel(&lvl))) { pct = lvl * 20; have_pct = 1; }
            }
            ptmuExit();
        }
        if (have_pct)
            add_row("Battery", "%u%%%s", pct, have_charge && charging ? " (charging)" : "");
    }

    // --- storage: SD card and the internal NAND media ---
    add_storage("SD",        SYSTEM_MEDIATYPE_SD);
    add_storage("CTR NAND",  SYSTEM_MEDIATYPE_CTR_NAND);
    add_storage("TWL NAND",  SYSTEM_MEDIATYPE_TWL_NAND);
    add_storage("TWL Photo", SYSTEM_MEDIATYPE_TWL_PHOTO);

    // --- Wi-Fi signal + MAC (shared-memory reads, always available) ---
    {
        u8 strength = osGetWifiStrength();           // 0-3 bars
        add_row("Wi-Fi", "%u/3 bars", strength);

        const u8 *mac = OS_SharedConfig->wifi_macaddr;
        add_row("MAC", "%02X:%02X:%02X:%02X:%02X:%02X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    // --- Local date/time from the RTC ---
    {
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        if (lt) {
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", lt);
            add_row("Time", "%s", buf);
        }
    }

    // --- render once: model-specific logo on the left, info on the right ---
    int logo_lines;
    const char **logo = select_logo(have_model ? (int)model : -1, &logo_lines);
    consoleClear();
    printf("\n");
    int total = logo_lines > row_count ? logo_lines : row_count;
    for (int i = 0; i < total; i++) {
        const char *l = i < logo_lines ? logo[i] : "";
        const char *r = i < row_count ? rows[i] : "";
        printf(" " CYAN "%-*s" RESET "  %s\n", LOGO_W, l, r);
    }
    printf("\n " RED "Press START to exit." RESET "\n");

    // --- main loop: the screen is static, so just present and handle input ---
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
