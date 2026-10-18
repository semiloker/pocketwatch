// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "ui/i18n.h"

typedef struct {
    const char *key, *text;
} entry;

static const entry g_english[PW_S_COUNT] = {
    [PW_S_GAME] = {"game", "Game"},
    [PW_S_RESUME_GAME] = {"resume_game", "Resume Game"},
    [PW_S_SAVE_STATE] = {"save_state", "Save State"},
    [PW_S_LOAD_STATE] = {"load_state", "Load State"},
    [PW_S_QUIT_GAME] = {"quit_game", "Quit Game"},
    [PW_S_SLOTS_USED] = {"slots_used", "%d of %d slots used"},
    [PW_S_REPLACES_RAM] = {"replaces_ram", "Replaces the state in memory."},
    [PW_S_SLOT_SAVED_AT] = {"slot_saved_at", "Save State %d, saved at %s"},
    [PW_S_SAVED_AT] = {"saved_at", "Saved at %s"},
    [PW_S_SAVED_AT_TIME] = {"saved_at_time", "Saved at %s (%u.%u s)"},
    [PW_S_NO_SAVED_STATE] = {"no_saved_state", "No saved state"},
    [PW_S_SAVING] = {"saving", "Saving..."},
    [PW_S_LOADING] = {"loading", "Loading..."},
    [PW_S_SAVING_PCT] = {"saving_pct", "Saving... %d%%"},
    [PW_S_LOADING_PCT] = {"loading_pct", "Loading... %d%%"},
    [PW_S_WRITE_FAILED] = {"write_failed", "The state could not be written to the Memory Stick."},
    [PW_S_SAVE_FAILED] = {"save_failed", "The state could not be saved."},
    [PW_S_LOAD_FAILED] = {"load_failed", "The state could not be loaded."},
    [PW_S_CANNOT_LOAD] = {"cannot_load", "This state cannot be loaded."},
    [PW_S_DETAILS_LOG] = {"details_log", "Details are in pocketwatch.log."},
    [PW_S_BUSY] = {"busy", "Another save or load is running."},
    [PW_S_CONFIRM_LOAD] = {"confirm_load", "Load the saved state?"},
    [PW_S_CONFIRM_LOAD2] = {"confirm_load2", "Unsaved progress will be lost."},
    [PW_S_CONFIRM_QUIT] = {"confirm_quit", "Do you want to quit the game?"},
    [PW_S_CHECK_FAILED] = {"check_failed", "The Memory Stick could not be checked."},
    [PW_S_CHECK_FAILED2] = {"check_failed2", "Do not save in the game: quit and restart the PSP."},
    [PW_S_SLOT_TITLE] = {"slot_title", "Save State %d"},
    [PW_S_NEW_SLOT] = {"new_slot", "New Save State (%d)"},
    [PW_S_NO_STATES] = {"no_states", "There are no saved states."},
    [PW_S_CONFIRM_OVERWRITE] = {"confirm_overwrite", "Overwrite Save State %d?"},
    [PW_S_CONFIRM_OVERWRITE2] = {"confirm_overwrite2", "The state saved there will be replaced."},
    [PW_S_CONFIRM_DELETE] = {"confirm_delete", "Delete Save State %d?"},
    [PW_S_YES] = {"yes", "Yes"},
    [PW_S_NO] = {"no", "No"},
    [PW_S_OK] = {"ok", "OK"},
    [PW_S_ENTER] = {"enter", "Enter"},
    [PW_S_BACK] = {"back", "Back"},
    [PW_S_DELETE] = {"delete", "Delete"},
    [PW_S_SIZE_MB] = {"size_mb", "%s   %u.%u MB"},
    [PW_S_E_NO_GAME] = {"e_no_game", "The game could not be identified."},
    [PW_S_E_IO] = {"e_io", "The Memory Stick could not be written or read."},
    [PW_S_E_EMPTY] = {"e_empty", "This slot is empty."},
    [PW_S_E_OTHER_GAME] = {"e_other_game", "This state belongs to another game or ISO."},
    [PW_S_E_OTHER_SETUP] = {"e_other_setup", "This state was saved with another setup."},
    [PW_S_E_DAMAGED] = {"e_damaged", "This state is damaged."},
    [PW_S_E_NO_STATE] = {"e_no_state", "There is no state to write."},
    [PW_S_E_MEMORY] = {"e_memory", "Not enough memory."},
    [PW_S_E_BLOCKED] = {"e_blocked", "The Memory Stick is locked until the PSP restarts."},
    [PW_S_G_BATTERY] = {"g_battery", "The battery is low."},
    [PW_S_G_USB] = {"g_usb", "Not available while USB mode is on."},
    [PW_S_G_WLAN] = {"g_wlan", "Not available while the wireless network is in use."},
    [PW_S_G_PLUGIN] = {"g_plugin", "Another save state plugin is running."},
    [PW_S_G_HIGHMEM] = {"g_highmem", "Not available for this game (extra memory mode)."},
    [PW_S_H_BATTERY] = {"h_battery", "Connect the AC adaptor."},
    [PW_S_H_USB] = {"h_usb", "Turn USB mode off."},
    [PW_S_H_WLAN] = {"h_wlan", "Leave the game's network mode first."},
    [PW_S_H_PLUGIN] = {"h_plugin", "Turn off Custom Home and PSPStates in PLUGINS.TXT."},
    [PW_S_H_HIGHMEM] = {"h_highmem", "Turn off ARK's high memory option for this game."},
#ifdef PW_I18N_XMB
    [PW_S_SETTINGS] = {"settings", "Settings"},
    [PW_S_SET_FAST] = {"set_fast", "Fast Save and Load"},
    [PW_S_SET_STATUS] = {"set_status", "Clock and Battery"},
    [PW_S_SET_LANGUAGE] = {"set_language", "Language"},
    [PW_S_SYSTEM_LANGUAGE] = {"system_language", "System Language"},
    [PW_S_ON] = {"on", "On"},
    [PW_S_OFF] = {"off", "Off"},
    [PW_S_SETTINGS_NOT_SAVED] = {"settings_not_saved", "The settings could not be saved."},
    [PW_S_SET_STYLE] = {"set_style", "Style"},
    [PW_S_STYLE_XMB] = {"style_xmb", "Classic (XMB)"},
    [PW_S_STYLE_MODERN] = {"style_modern", "Modern"},
    [PW_S_STYLE_MINIMAL] = {"style_minimal", "Minimal"},
    [PW_S_BATTERY] = {"battery", "Battery"},
    [PW_S_BAT_CHARGE] = {"bat_charge", "Charge"},
    [PW_S_BAT_STATUS] = {"bat_status", "Status"},
    [PW_S_BAT_ON_BATTERY] = {"bat_on_battery", "On battery"},
    [PW_S_BAT_CHARGING] = {"bat_charging", "Charging"},
    [PW_S_BAT_ON_POWER] = {"bat_on_power", "On external power"},
    [PW_S_BAT_NONE] = {"bat_none", "No battery"},
    [PW_S_BAT_LEFT] = {"bat_left", "Time left"},
    [PW_S_BAT_TO_FULL] = {"bat_to_full", "Time to full"},
    [PW_S_BAT_ESTIMATING] = {"bat_estimating", "Estimating..."},
    [PW_S_BAT_ABOUT] = {"bat_about", "about %s"},
    [PW_S_BAT_SYSTEM] = {"bat_system", "%s (system)"},
    [PW_S_BAT_VOLTAGE] = {"bat_voltage", "Voltage"},
    [PW_S_BAT_CURRENT] = {"bat_current", "Current"},
    [PW_S_BAT_TEMPERATURE] = {"bat_temperature", "Temperature"},
    [PW_S_BAT_CAPACITY] = {"bat_capacity", "Capacity"},
    [PW_S_BAT_CAPACITY_V] = {"bat_capacity_v", "%d of %d mAh"},
    [PW_S_BAT_CYCLES] = {"bat_cycles", "Charge cycles"},
    [PW_S_BAT_GAUGE] = {"bat_gauge", "Measured by the battery"},
    [PW_S_BAT_NA] = {"bat_na", "Not available"},
    [PW_S_DUR_HM] = {"dur_hm", "%d h %02d min"},
    [PW_S_DUR_M] = {"dur_m", "%d min"},
    [PW_S_PLAYTIME] = {"playtime", "Play Time"},
    [PW_S_PT_SESSION] = {"pt_session", "This session"},
    [PW_S_PT_TOTAL] = {"pt_total", "Total"},
    [PW_S_PT_SESSIONS] = {"pt_sessions", "Sessions"},
    [PW_S_PT_LONGEST] = {"pt_longest", "Longest session"},
    [PW_S_PT_AVERAGE] = {"pt_average", "Average session"},
    [PW_S_PT_LAST] = {"pt_last", "Last played"},
    [PW_S_PT_SINCE] = {"pt_since", "Recorded since"},
    [PW_S_PT_ALL_GAMES] = {"pt_all_games", "All games"},
    [PW_S_PT_NOTE] = {"pt_note", "Counted only while Pocketwatch runs"},
    [PW_S_PT_DESC] = {"pt_desc", "%s in total"},
    [PW_S_PT_NONE] = {"pt_none", "Nothing recorded yet"},
    [PW_S_HM_GAMES] = {"hm_games", "Games"},
    [PW_S_HM_SYSTEM] = {"hm_system", "System"},
    [PW_S_HM_PLAY] = {"hm_play", "Play"},
    [PW_S_HM_EXIT] = {"hm_exit", "Exit to the XMB"},
    [PW_S_HM_NO_GAMES] = {"hm_no_games", "No games in ms0:/ISO"},
    [PW_S_HM_STATES] = {"hm_states", "Save states: %d"},
    [PW_S_HM_PLAYED] = {"hm_played", "Played %s"},
    [PW_S_HM_SECTION] = {"hm_section", "Section"},
    [PW_S_HM_FREE] = {"hm_free", "Free space"},
    [PW_S_HM_VERSION] = {"hm_version", "Version"},
    [PW_S_HM_INSTALLED] = {"hm_installed", "Games on the card"},
#endif
};

static const char *g_text[PW_S_COUNT]; /* NULL: English */
static char g_pool[PW_I18N_POOL];      /* the translated texts (kernel RAM: part of a state) */

const char *pw_tr(int id)
{
    if (id < 0 || id >= PW_S_COUNT)
        return "";
    return g_text[id] ? g_text[id] : g_english[id].text ? g_english[id].text : "";
}

const char *pw_i18n_key(int id)
{
    return id >= 0 && id < PW_S_COUNT ? g_english[id].key : 0;
}

const char *pw_i18n_english(int id)
{
    return id >= 0 && id < PW_S_COUNT ? g_english[id].text : 0;
}

const char *pw_i18n_code(int psp_language)
{
    /* PSP_SYSTEMPARAM_LANGUAGE_*: 0 ja, 1 en, 2 fr, 3 es, 4 de, 5 it, 6 nl, 7 pt, 8 ru, 9 ko,
       10 zh (traditional), 11 zh (simplified). Japanese, Korean and Chinese also need their
       system fonts (jpn0, kr0, gb3s1518), which are not loaded yet. */
    static const char *const codes[] = {0, 0, "fr", "es", "de", "it", "nl", "pt", "ru"};

    if (psp_language < 0 || psp_language >= (int)(sizeof(codes) / sizeof(codes[0])))
        return 0;
    return codes[psp_language];
}

void pw_i18n_reset(void)
{
    for (int i = 0; i < PW_S_COUNT; i++)
        g_text[i] = 0;
}

/* The conversion letters of a printf format, in order ("%d of %d" -> "dd"); %% is not one. */
static int conversions(const char *s, char *out, int cap)
{
    int n = 0;

    for (; *s; s++) {
        if (*s != '%')
            continue;
        s++;
        if (*s == '%')
            continue;
        while (*s && strchr("-+ #0123456789.lhz", *s))
            s++;
        if (!*s || n >= cap - 1)
            return -1;
        out[n++] = *s;
    }
    out[n] = 0;
    return n;
}

int pw_i18n_same_format(const char *a, const char *b)
{
    char ca[16], cb[16];

    return conversions(a, ca, sizeof(ca)) >= 0 && conversions(b, cb, sizeof(cb)) >= 0 &&
           strcmp(ca, cb) == 0;
}

static char *trim(char *s, char *end)
{
    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        end--;
    *end = 0;
    return s;
}

int pw_i18n_load(char *text, unsigned int len, pw_i18n_covers covers, void *ctx)
{
    char *p = text, *end = text + len;
    unsigned int used = 0;
    int taken = 0;

    pw_i18n_reset();
    if (len >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB &&
        (unsigned char)p[2] == 0xBF)
        p += 3; /* a UTF-8 BOM from a Windows editor */
    while (p < end) {
        char *nl = memchr(p, '\n', (unsigned int)(end - p));
        char *line_end = nl ? nl : end, *eq, *key, *val;

        eq = memchr(p, '=', (unsigned int)(line_end - p));
        if (*p != '#' && eq != 0) {
            key = trim(p, eq);
            val = trim(eq + 1, line_end);
            for (int i = 0; i < PW_I18N_HELD; i++)
                if (strcmp(key, g_english[i].key) == 0) {
                    if (val[0] && pw_i18n_same_format(val, g_english[i].text)) {
                        unsigned int n = (unsigned int)strlen(val) + 1;

                        if (used + n > sizeof(g_pool)) {
                            pw_i18n_reset();
                            return -3;
                        }
                        memcpy(g_pool + used, val, n);
                        if (g_text[i] == 0)
                            taken++;
                        g_text[i] = g_pool + used;
                        used += n;
                    }
                    break;
                }
        } else if (nl) {
            *nl = 0;
        }
        if (!nl)
            break;
        p = nl + 1;
    }
    if (taken == 0) {
        pw_i18n_reset();
        return -1;
    }
    if (covers)
        for (int i = 0; i < PW_S_COUNT; i++)
            if (g_text[i] && !covers(g_text[i], ctx)) {
                pw_i18n_reset();
                return -2;
            }
    return taken;
}
