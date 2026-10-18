// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_I18N_H
#define PW_I18N_H

/*
 * The interface's text in the system language (UI-REQ-008). English is built in; a translation is
 * a UTF-8 text file of "key = text" lines (plugin/lang/<code>.txt, installed next to the PRX) read
 * once at plugin start, so the PRX does not grow with every language. The texts taken are copied
 * into a small pool in kernel RAM, which is part of every state: a state restores its own
 * language, whatever the system language is when it loads. Portable: the PSP side (ui/lang.c)
 * finds and reads the file, this module checks and keeps it.
 */

/* Bytes of translated text: the game keeps its own strings in kernel RAM (Russian ~2.4 KB); the
   XMB plugin and Home (PW_I18N_XMB, user memory) take every string (Russian ~3.9 KB, RISK-019). */
#ifdef PW_I18N_XMB
#define PW_I18N_POOL 8192
#else
#define PW_I18N_POOL 4096
#endif

enum {
    PW_S_GAME,
    PW_S_RESUME_GAME,
    PW_S_SAVE_STATE,
    PW_S_LOAD_STATE,
    PW_S_QUIT_GAME,
    PW_S_SLOTS_USED,
    PW_S_REPLACES_RAM,
    PW_S_SLOT_SAVED_AT,
    PW_S_SAVED_AT,
    PW_S_SAVED_AT_TIME,
    PW_S_NO_SAVED_STATE,
    PW_S_SAVING,
    PW_S_LOADING,
    PW_S_SAVING_PCT,
    PW_S_LOADING_PCT,
    PW_S_WRITE_FAILED,
    PW_S_SAVE_FAILED,
    PW_S_LOAD_FAILED,
    PW_S_CANNOT_LOAD,
    PW_S_DETAILS_LOG,
    PW_S_BUSY,
    PW_S_CONFIRM_LOAD,
    PW_S_CONFIRM_LOAD2,
    PW_S_CONFIRM_QUIT,
    PW_S_CHECK_FAILED,
    PW_S_CHECK_FAILED2,
    PW_S_SLOT_TITLE,
    PW_S_NEW_SLOT,
    PW_S_NO_STATES,
    PW_S_CONFIRM_OVERWRITE,
    PW_S_CONFIRM_OVERWRITE2,
    PW_S_CONFIRM_DELETE,
    PW_S_YES,
    PW_S_NO,
    PW_S_OK,
    PW_S_ENTER,
    PW_S_BACK,
    PW_S_DELETE,
    PW_S_SIZE_MB,
    PW_S_E_NO_GAME,
    PW_S_E_IO,
    PW_S_E_EMPTY,
    PW_S_E_OTHER_GAME,
    PW_S_E_OTHER_SETUP,
    PW_S_E_DAMAGED,
    PW_S_E_NO_STATE,
    PW_S_E_MEMORY,
    PW_S_E_BLOCKED,
    PW_S_G_BATTERY,
    PW_S_G_USB,
    PW_S_G_WLAN,
    PW_S_G_PLUGIN,
    PW_S_G_HIGHMEM,
    PW_S_H_BATTERY,
    PW_S_H_USB,
    PW_S_H_WLAN,
    PW_S_H_PLUGIN,
    PW_S_H_HIGHMEM,
    PW_S_SETTINGS,
    PW_S_SET_FAST,
    PW_S_SET_STATUS,
    PW_S_SET_LANGUAGE,
    PW_S_SYSTEM_LANGUAGE,
    PW_S_ON,
    PW_S_OFF,
    PW_S_SETTINGS_NOT_SAVED,
    PW_S_SET_STYLE,
    PW_S_STYLE_XMB,
    PW_S_STYLE_MODERN,
    PW_S_STYLE_MINIMAL,
    PW_S_BATTERY,
    PW_S_BAT_CHARGE,
    PW_S_BAT_STATUS,
    PW_S_BAT_ON_BATTERY,
    PW_S_BAT_CHARGING,
    PW_S_BAT_ON_POWER,
    PW_S_BAT_NONE,
    PW_S_BAT_LEFT,
    PW_S_BAT_TO_FULL,
    PW_S_BAT_ESTIMATING,
    PW_S_BAT_ABOUT,
    PW_S_BAT_SYSTEM,
    PW_S_BAT_VOLTAGE,
    PW_S_BAT_CURRENT,
    PW_S_BAT_TEMPERATURE,
    PW_S_BAT_CAPACITY,
    PW_S_BAT_CAPACITY_V,
    PW_S_BAT_CYCLES,
    PW_S_BAT_GAUGE,
    PW_S_BAT_NA,
    PW_S_DUR_HM,
    PW_S_DUR_M,
    PW_S_PLAYTIME,
    PW_S_PT_SESSION,
    PW_S_PT_TOTAL,
    PW_S_PT_SESSIONS,
    PW_S_PT_LONGEST,
    PW_S_PT_AVERAGE,
    PW_S_PT_LAST,
    PW_S_PT_SINCE,
    PW_S_PT_ALL_GAMES,
    PW_S_PT_NOTE,
    PW_S_PT_DESC,
    PW_S_PT_NONE,
    PW_S_HM_GAMES,
    PW_S_HM_SYSTEM,
    PW_S_HM_PLAY,
    PW_S_HM_EXIT,
    PW_S_HM_NO_GAMES,
    PW_S_HM_STATES,
    PW_S_HM_PLAYED,
    PW_S_HM_SECTION,
    PW_S_HM_FREE,
    PW_S_HM_VERSION,
    PW_S_HM_INSTALLED,
    PW_S_COUNT
};

/* Strings from PW_S_SETTINGS on are shown only by the XMB plugin (vsh/, built with PW_I18N_XMB):
   the game build keeps neither their English nor their translations (kernel RAM and the 4 KB pool,
   RISK-019); pw_tr gives "" for them there. */
#define PW_S_XMB_FIRST PW_S_SETTINGS
#ifdef PW_I18N_XMB
#define PW_I18N_HELD PW_S_COUNT
#else
#define PW_I18N_HELD PW_S_XMB_FIRST
#endif

/* The string for id in the current language (never NULL). */
const char *pw_tr(int id);

/* The file name code for a PSP system language (sceImposeGetLanguageMode), or NULL for English
   and for languages without a translation yet. */
const char *pw_i18n_code(int psp_language);

/* Can the fonts draw this UTF-8 string? (supplied by the caller: the system font's char map) */
typedef int (*pw_i18n_covers)(const char *utf8, void *ctx);

/*
 * Take a translation. text (len bytes, plus room for one more: text[len] is written) is parsed in
 * place; what is taken is copied, so text may be freed afterwards.
 * A line whose key is unknown, or whose printf conversions differ from the English string's, is
 * skipped; strings the file lacks stay English. The whole file is refused when covers says the
 * fonts lack a glyph of any taken string (mixed scripts would be unreadable) or when it does not
 * fit PW_I18N_POOL. Returns the number of strings taken (> 0), or < 0 and English everywhere.
 */
int pw_i18n_load(char *text, unsigned int len, pw_i18n_covers covers, void *ctx);
void pw_i18n_reset(void); /* English */

/* For tests and tools. */
const char *pw_i18n_key(int id);
const char *pw_i18n_english(int id);
int pw_i18n_same_format(const char *a, const char *b);

#endif
