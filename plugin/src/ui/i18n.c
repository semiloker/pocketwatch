// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "ui/i18n.h"

typedef struct {
    const char *key, *text;
} entry;

static const entry g_english[PW_I18N_HELD] = {
    /* this build's tiers only (i18n.h) */
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
    [PW_S_SCREENSHOT] = {"screenshot", "Screenshot"},
    [PW_S_SHOT_SAVED] = {"shot_saved", "Saved to Photos: PICTURE/%s"},
#if defined(PW_I18N_XMB) || defined(PW_I18N_HOME)
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
    [PW_S_SET_START] = {"set_start", "Start screen"},
    [PW_S_START_XMB] = {"start_xmb", "XMB"},
    [PW_S_START_HOME] = {"start_home", "Pocketwatch Home"},
#endif
#ifdef PW_I18N_HOME
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
    [PW_S_HM_HOMEBREW] = {"hm_homebrew", "Homebrew"},
    [PW_S_HM_PS1] = {"hm_ps1", "PlayStation"},
    [PW_S_HM_CLOSE] = {"hm_close", "Close"},
    [PW_S_HM_SLEEP] = {"hm_sleep", "Sleep"},
    [PW_S_HM_POWER_OFF] = {"hm_power_off", "Turn Off"},
    [PW_S_HM_USB] = {"hm_usb", "USB Connection"},
    [PW_S_HM_USB_CABLE] = {"hm_usb_cable", "Connect the USB cable to a computer."},
    [PW_S_HM_USB_WAIT] = {"hm_usb_wait", "Connecting to the computer..."},
    [PW_S_HM_USB_ON] = {"hm_usb_on", "Connected. The computer has the Memory Stick."},
    [PW_S_HM_USB_FAIL] = {"hm_usb_fail", "USB could not be started."},
    [PW_S_HM_USB_END] = {"hm_usb_end", "End"},
    [PW_S_HM_USB_AGAIN] = {"hm_usb_again", "Connect Again"},
    [PW_S_HM_STORAGE] = {"hm_storage", "Memory Stick"},
    [PW_S_HS_POCKETWATCH] = {"hs_pocketwatch", "Pocketwatch"},
    [PW_S_HS_NICKNAME] = {"hs_nickname", "Nickname"},
    [PW_S_HS_SYS_LANGUAGE] = {"hs_sys_language", "System Language"},
    [PW_S_HS_DATE_FORMAT] = {"hs_date_format", "Date Format"},
    [PW_S_HS_TIME_FORMAT] = {"hs_time_format", "Time Format"},
    [PW_S_HS_TIME_ZONE] = {"hs_time_zone", "Time Zone"},
    [PW_S_HS_SUMMER_TIME] = {"hs_summer_time", "Summer Time"},
    [PW_S_HS_WLAN_SAVE] = {"hs_wlan_save", "WLAN Power Save"},
    [PW_S_HS_SCREEN_SOUND] = {"hs_screen_sound", "Screen and Sound"},
    [PW_S_HS_BRIGHTNESS] = {"hs_brightness", "Backlight Brightness"},
    [PW_S_HS_AVLS] = {"hs_avls", "AVLS"},
    [PW_S_HS_24H] = {"hs_24h", "24-Hour Clock"},
    [PW_S_HS_12H] = {"hs_12h", "12-Hour Clock"},
    [PW_S_HS_LANG_JA] = {"hs_lang_ja", "Japanese"},
    [PW_S_HS_LANG_KO] = {"hs_lang_ko", "Korean"},
    [PW_S_HS_LANG_ZHT] = {"hs_lang_zht", "Chinese (Traditional)"},
    [PW_S_HS_LANG_ZHS] = {"hs_lang_zhs", "Chinese (Simplified)"},
    [PW_S_HM_MODEL] = {"hm_model", "Model"},
    [PW_S_HM_FIRMWARE] = {"hm_firmware", "System Software"},
    [PW_S_HM_CFW] = {"hm_cfw", "Custom Firmware"},
    [PW_S_HM_MAC] = {"hm_mac", "MAC Address"},
    [PW_S_HM_OPTIONS] = {"hm_options", "Options"},
    [PW_S_HM_SAVED_DATA] = {"hm_saved_data", "Saved Data"},
    [PW_S_HM_CONTINUE] = {"hm_continue", "Continue"},
    [PW_S_HM_MUSIC] = {"hm_music", "Music"},
    [PW_S_HM_MU_IDLE] = {"hm_mu_idle", "Nothing playing"},
    [PW_S_HM_MU_PAUSED] = {"hm_mu_paused", "Paused"},
    [PW_S_HM_MU_NONE] = {"hm_mu_none", "No music in ms0:/MUSIC"},
    [PW_S_HM_PHOTOS] = {"hm_photos", "Photos"},
    [PW_S_HM_VIEW] = {"hm_view", "View"},
    [PW_S_HM_PH_NONE] = {"hm_ph_none", "No pictures in ms0:/PICTURE"},
    [PW_S_HS_SORT] = {"hs_sort", "Sort Games"},
    [PW_S_HS_SORT_NAME] = {"hs_sort_name", "By Name"},
    [PW_S_HS_SORT_RECENT] = {"hs_sort_recent", "Recently Played"},
    [PW_S_HM_INFORMATION] = {"hm_information", "Information"},
    [PW_S_HM_DISC_ID] = {"hm_disc_id", "Disc ID"},
    [PW_S_HM_SIZE] = {"hm_size", "Size"},
    [PW_S_HM_FILE] = {"hm_file", "File"},
    [PW_S_HM_SLIDESHOW] = {"hm_slideshow", "Slideshow"},
    [PW_S_HM_WALLPAPER] = {"hm_wallpaper", "Wallpaper"},
    [PW_S_HM_WP_DEFAULT] = {"hm_wp_default", "Pocketwatch"},
    [PW_S_HM_WP_SET] = {"hm_wp_set", "Set as wallpaper"},
    [PW_S_HS_SLIDESHOW] = {"hs_slideshow", "Slideshow Speed"},
    [PW_S_HS_SLIDE_NORMAL] = {"hs_slide_normal", "Normal"},
    [PW_S_HS_SLIDE_FAST] = {"hs_slide_fast", "Fast"},
    [PW_S_HS_SLIDE_SLOW] = {"hs_slide_slow", "Slow"},
    [PW_S_HS_SOUNDS] = {"hs_sounds", "Key Sounds"},
    [PW_S_HM_BROWSER] = {"hm_browser", "Internet Browser"},
    [PW_S_HM_USB_BUSY] = {"hm_usb_busy", "Another plugin holds the USB port (RemoteJoyLite?). Turn "
                                         "it off for games, then try again."},
    [PW_S_HS_CAT_LOOK] = {"hs_cat_look", "Appearance"},
    [PW_S_HS_CAT_MEDIA] = {"hs_cat_media", "Music and Photos"},
    [PW_S_HS_CAT_SOUND] = {"hs_cat_sound", "Sound and Display"},
    [PW_S_HS_CAT_TIME] = {"hs_cat_time", "Date and Time"},
    [PW_S_HS_CAT_NETWORK] = {"hs_cat_network", "Network"},
    [PW_S_HS_THEME] = {"hs_theme", "Theme"},
    [PW_S_HS_THEME_DARK] = {"hs_theme_dark", "Dark"},
    [PW_S_HS_THEME_GLASS] = {"hs_theme_glass", "Glass"},
    [PW_S_HS_THEME_OLED] = {"hs_theme_oled", "OLED Black"},
    [PW_S_HS_THEME_PAPER] = {"hs_theme_paper", "Paper"},
    [PW_S_HS_ACCENT] = {"hs_accent", "Accent Colour"},
    [PW_S_HS_ACC_BLUE] = {"hs_acc_blue", "Blue"},
    [PW_S_HS_ACC_VIOLET] = {"hs_acc_violet", "Violet"},
    [PW_S_HS_ACC_PINK] = {"hs_acc_pink", "Pink"},
    [PW_S_HS_ACC_RED] = {"hs_acc_red", "Red"},
    [PW_S_HS_ACC_ORANGE] = {"hs_acc_orange", "Orange"},
    [PW_S_HS_ACC_GOLD] = {"hs_acc_gold", "Gold"},
    [PW_S_HS_ACC_GREEN] = {"hs_acc_green", "Green"},
    [PW_S_HS_ACC_TEAL] = {"hs_acc_teal", "Teal"},
    [PW_S_HS_ACC_GRAPHITE] = {"hs_acc_graphite", "Graphite"},
    [PW_S_HS_BACKGROUND] = {"hs_background", "Background"},
    [PW_S_HS_BG_AUTO] = {"hs_bg_auto", "As the Style"},
    [PW_S_HS_BG_SKY] = {"hs_bg_sky", "XMB Sky"},
    [PW_S_HS_BG_GRADIENT] = {"hs_bg_gradient", "Gradient"},
    [PW_S_HS_BG_PLAIN] = {"hs_bg_plain", "Plain"},
    [PW_S_HS_SKY] = {"hs_sky", "Sky Colour"},
    [PW_S_HS_SKY_SYSTEM] = {"hs_sky_system", "As the XMB"},
    [PW_S_MONTH_1] = {"month_1", "January"},
    [PW_S_MONTH_2] = {"month_2", "February"},
    [PW_S_MONTH_3] = {"month_3", "March"},
    [PW_S_MONTH_4] = {"month_4", "April"},
    [PW_S_MONTH_5] = {"month_5", "May"},
    [PW_S_MONTH_6] = {"month_6", "June"},
    [PW_S_MONTH_7] = {"month_7", "July"},
    [PW_S_MONTH_8] = {"month_8", "August"},
    [PW_S_MONTH_9] = {"month_9", "September"},
    [PW_S_MONTH_10] = {"month_10", "October"},
    [PW_S_MONTH_11] = {"month_11", "November"},
    [PW_S_MONTH_12] = {"month_12", "December"},
    [PW_S_HS_WAVE] = {"hs_wave", "Wave"},
    [PW_S_HS_PERCENT] = {"hs_percent", "Battery Percentage"},
    [PW_S_HS_MOTION] = {"hs_motion", "Animations"},
    [PW_S_HS_MOTION_FAST] = {"hs_motion_fast", "Quick"},
    [PW_S_HS_MOTION_CALM] = {"hs_motion_calm", "Gentle"},
    [PW_S_HS_REPEAT] = {"hs_repeat", "Repeat"},
    [PW_S_HS_REPEAT_ALL] = {"hs_repeat_all", "All Tracks"},
    [PW_S_HS_REPEAT_ONE] = {"hs_repeat_one", "One Track"},
    [PW_S_HS_SHUFFLE] = {"hs_shuffle", "Shuffle"},
    [PW_S_HM_ABOUT] = {"hm_about", "About This PSP"},
    [PW_S_HM_POWER] = {"hm_power", "Power"},
    [PW_S_HM_FREE_MB] = {"hm_free_mb", "%d MB free"},
    [PW_S_HM_OPEN] = {"hm_open", "Open"},
    [PW_S_HM_CHANGE] = {"hm_change", "Change"},
    [PW_S_HM_CANCEL] = {"hm_cancel", "Cancel"},
    [PW_S_HM_STATES_PAGE] = {"hm_states_page", "Save States"},
    [PW_S_HM_NO_SAVES] = {"hm_no_saves", "No saved data for this game."},
    [PW_S_HM_DEL_SAVE_Q] = {"hm_del_save_q", "Delete this saved data? This cannot be undone."},
    [PW_S_HM_DEL_PHOTO_Q] = {"hm_del_photo_q", "Delete this photo? This cannot be undone."},
    [PW_S_HM_DELETED] = {"hm_deleted", "Deleted"},
    [PW_S_HM_DEL_FAIL] = {"hm_del_fail", "Could not delete it."},
    [PW_S_HM_SD_GAME] = {"hm_sd_game", "Game"},
    [PW_S_HM_SD_DETAIL] = {"hm_sd_detail", "Details"},
    [PW_S_HM_SD_DATE] = {"hm_sd_date", "Saved"},
    [PW_S_HM_SD_FOLDER] = {"hm_sd_folder", "Folder"},
    [PW_S_HM_N_SAVES] = {"hm_n_saves", "%d saved"},
    [PW_S_HM_NOW] = {"hm_now", "Now Playing"},
    [PW_S_HM_PAUSE] = {"hm_pause", "Pause"},
    [PW_S_HM_STOP] = {"hm_stop", "Stop"},
    [PW_S_HM_MU_KEYS] = {"hm_mu_keys", "Left / Right: track, hold: seek"},
    [PW_S_HM_UNKNOWN_ARTIST] = {"hm_unknown_artist", "Unknown Artist"},
    [PW_S_HM_ALL_PHOTOS] = {"hm_all_photos", "All Photos"},
    [PW_S_HM_N_PHOTOS] = {"hm_n_photos", "%d photos"},
    [PW_S_HM_EDIT] = {"hm_edit", "Edit"},
    [PW_S_HM_ED_ROTATE] = {"hm_ed_rotate", "Rotate"},
    [PW_S_HM_ED_MIRROR] = {"hm_ed_mirror", "Mirror"},
    [PW_S_HM_ED_FILTER] = {"hm_ed_filter", "Filter"},
    [PW_S_HM_ED_LIGHT] = {"hm_ed_light", "Brightness"},
    [PW_S_HM_ED_CONTRAST] = {"hm_ed_contrast", "Contrast"},
    [PW_S_HM_ED_SAT] = {"hm_ed_sat", "Saturation"},
    [PW_S_HM_ED_WARMTH] = {"hm_ed_warmth", "Warmth"},
    [PW_S_HM_ED_VIGNETTE] = {"hm_ed_vignette", "Vignette"},
    [PW_S_HM_ED_SAVE] = {"hm_ed_save", "Save a Copy"},
    [PW_S_HM_ED_KEYS] = {"hm_ed_keys", "Left / Right: tool   Up / Down: change"},
    [PW_S_HM_ED_SAVED] = {"hm_ed_saved", "Saved as a new photo"},
    [PW_S_HM_ED_FAILED] = {"hm_ed_failed", "Could not save the photo."},
    [PW_S_HM_FL_NONE] = {"hm_fl_none", "Original"},
    [PW_S_HM_FL_MONO] = {"hm_fl_mono", "Mono"},
    [PW_S_HM_FL_SEPIA] = {"hm_fl_sepia", "Sepia"},
    [PW_S_HM_FL_VIVID] = {"hm_fl_vivid", "Vivid"},
    [PW_S_HM_FL_FADE] = {"hm_fl_fade", "Faded"},
    [PW_S_HM_FL_COOL] = {"hm_fl_cool", "Cool"},
    [PW_S_HM_FL_NOIR] = {"hm_fl_noir", "Noir"},
    [PW_S_HM_FL_RETRO] = {"hm_fl_retro", "Retro"},
    [PW_S_HM_WP_DONE] = {"hm_wp_done", "Set as wallpaper"},
    [PW_S_HM_SCREENSHOTS] = {"hm_screenshots", "Screenshots"},
    [PW_S_HS_CLOCK] = {"hs_clock", "Clock"},
    [PW_S_HS_CLOCK_DATE] = {"hs_clock_date", "Date and Time"},
    [PW_S_HS_CLOCK_TIME] = {"hs_clock_time", "Time Only"},
    [PW_S_HS_CLOCK_DAY] = {"hs_clock_day", "Weekday and Time"},
    [PW_S_WD_0] = {"wd_0", "Sun"},
    [PW_S_WD_1] = {"wd_1", "Mon"},
    [PW_S_WD_2] = {"wd_2", "Tue"},
    [PW_S_WD_3] = {"wd_3", "Wed"},
    [PW_S_WD_4] = {"wd_4", "Thu"},
    [PW_S_WD_5] = {"wd_5", "Fri"},
    [PW_S_WD_6] = {"wd_6", "Sat"},
    [PW_S_HS_HOLD] = {"hs_hold", "Screen Off with HOLD"},
    [PW_S_HS_HOLD_MUSIC] = {"hs_hold_music", "While Music Plays"},
    [PW_S_HS_HOLD_ALWAYS] = {"hs_hold_always", "Always"},
    [PW_S_HS_TIMER] = {"hs_timer", "Sleep Timer"},
    [PW_S_HS_TIMER_MIN] = {"hs_timer_min", "%d min"},
    [PW_S_HM_TIMER_LEFT] = {"hm_timer_left", "Sleep in %d min"},
    [PW_S_HM_FAVORITE] = {"hm_favorite", "Favourite"},
    [PW_S_HM_FAV_ON] = {"hm_fav_on", "Added to favourites"},
    [PW_S_HM_FAV_OFF] = {"hm_fav_off", "Removed from favourites"},
    [PW_S_HS_SORT_TYPE] = {"hs_sort_type", "By Kind"},
    [PW_S_HS_BG_AURORA] = {"hs_bg_aurora", "Aurora"},
    [PW_S_HS_ENTER] = {"hs_enter", "Enter Button"},
    [PW_S_HS_BTN_CIRCLE] = {"hs_btn_circle", "Circle"},
    [PW_S_HS_BTN_CROSS] = {"hs_btn_cross", "Cross"},
    [PW_S_HS_ADHOC] = {"hs_adhoc", "Ad Hoc Channel"},
    [PW_S_HS_AUTOMATIC] = {"hs_automatic", "Automatic"},
    [PW_S_DUR_S] = {"dur_s", "%d s"},
    [PW_S_HM_SEEK] = {"hm_seek", "Seek"},
    [PW_S_HM_TRACK] = {"hm_track", "Track"},
    [PW_S_KB_SPACE] = {"kb_space", "Space"},
    [PW_S_KB_ERASE] = {"kb_erase", "Erase"},
    [PW_S_KB_CURSOR] = {"kb_cursor", "Cursor"},
    [PW_S_KB_DONE] = {"kb_done", "Done"},
    [PW_S_KB_ASK] = {"kb_ask", "Save the changes?"},
    [PW_S_KB_SAVE] = {"kb_save", "Save"},
    [PW_S_KB_DISCARD] = {"kb_discard", "Discard"},
    [PW_S_KB_KEEP] = {"kb_keep", "Keep Editing"},
    [PW_S_CLK_NET] = {"clk_net", "Set Time from Internet"},
    [PW_S_CLK_WORKING] = {"clk_working", "Connecting..."},
    [PW_S_CLK_SET] = {"clk_set", "Set at %s"},
    [PW_S_CLK_NO_WIFI] = {"clk_no_wifi", "Wi-Fi switch is off"},
    [PW_S_CLK_NO_NET] = {"clk_no_net", "No saved connection"},
    [PW_S_CLK_FAILED] = {"clk_failed", "Could not get the time"},
    [PW_S_CLK_RESTORED] = {"clk_restored",
                           "The clock was lost: set to the last time Pocketwatch saw"},
    [PW_S_CLK_LOST] = {"clk_lost", "The clock was lost: set it in Settings, Date and Time"},
    [PW_S_CLK_SYNCED] = {"clk_synced", "Time set from the internet"},
    [PW_S_BAT_MEASURED] = {"bat_measured", "Measured by Pocketwatch"},
    [PW_S_BAT_MEASURED_V] = {"bat_measured_v", "%d mAh (%d%% of %d)"},
    [PW_S_BAT_NOT_MEASURED] = {"bat_not_measured", "Not measured yet"},
    [PW_S_BAT_LASTED] = {"bat_lasted", "Lasted"},
    [PW_S_BAT_CAL] = {"bat_cal", "Calibration"},
    [PW_S_BAT_CAL_START] = {"bat_cal_start", "Start"},
    [PW_S_BAT_CAL_CHARGE] = {"bat_cal_charge", "Charge it full"},
    [PW_S_BAT_CAL_UNPLUG] = {"bat_cal_unplug", "Full: unplug the charger"},
    [PW_S_BAT_CAL_RUN] = {"bat_cal_run", "Measuring: leave Home on"},
    [PW_S_BAT_CAL_DONE] = {"bat_cal_done", "Battery measured: %d mAh"},
    [PW_S_BAT_CAL_SHORT] = {"bat_cal_short", "Calibration stopped: too short to measure"},
    [PW_S_HS_PERF] = {"hs_perf", "Performance Overlay"},
    [PW_S_HS_PERF_GAME] = {"hs_perf_game", "Overlay in Games"},
    [PW_S_HS_PERF_FPS] = {"hs_perf_fps", "FPS"},
    [PW_S_HS_PERF_FULL] = {"hs_perf_full", "Full"},
    [PW_S_HS_PERF_NEXT] = {"hs_perf_next", "From the next game"},
    [PW_S_WIFI_CONNS] = {"wifi_conns", "Wi-Fi Connections"},
    [PW_S_WIFI_SAVED] = {"wifi_saved", "Saved connections"},
    [PW_S_WIFI_NEARBY] = {"wifi_nearby", "Networks nearby"},
    [PW_S_WIFI_SEARCH] = {"wifi_search", "Search for Networks"},
    [PW_S_WIFI_SEARCHING] = {"wifi_searching", "Searching..."},
    [PW_S_WIFI_FOUND] = {"wifi_found", "%d found"},
    [PW_S_WIFI_NONE_FOUND] = {"wifi_none_found", "None found"},
    [PW_S_WIFI_SEARCH_FAILED] = {"wifi_search_failed", "Search failed"},
    [PW_S_WIFI_NOTE] = {"wifi_note", "PSP: 2.4 GHz only, WPA2 / WPA / open"},
    [PW_S_WIFI_OPEN] = {"wifi_open", "Open"},
    [PW_S_WIFI_WEP] = {"wifi_wep", "WEP"},
    [PW_S_WIFI_WPA] = {"wifi_wpa", "WPA (TKIP)"},
    [PW_S_WIFI_WPA2] = {"wifi_wpa2", "WPA2 (AES)"},
    [PW_S_WIFI_UNSUP] = {"wifi_unsup", "Not supported by the PSP"},
    [PW_S_WIFI_IS_SAVED] = {"wifi_is_saved", "Saved"},
    [PW_S_WIFI_NETWORK] = {"wifi_network", "Network"},
    [PW_S_WIFI_SECURITY] = {"wifi_security", "Security"},
    [PW_S_WIFI_TEST] = {"wifi_test", "Test Connection"},
    [PW_S_WIFI_CONNECTING] = {"wifi_connecting", "Connecting..."},
    [PW_S_WIFI_CONNECTED] = {"wifi_connected", "Connected: %s"},
    [PW_S_WIFI_FAILED] = {"wifi_failed", "Could not connect"},
    [PW_S_WIFI_PASSWORD] = {"wifi_password", "Password for %s"},
    [PW_S_WIFI_KEY_BAD] = {"wifi_key_bad", "The password needs 8 to 63 characters"},
    [PW_S_WIFI_NOT_SAVED] = {"wifi_not_saved", "Could not save the connection"},
    [PW_S_WIFI_FULL] = {"wifi_full", "Ten connections are saved: delete one first"},
    [PW_S_WIFI_WEP_XMB] = {"wifi_wep_xmb", "WEP: set it up in the XMB's Network Settings"},
    [PW_S_WIFI_DELETE_Q] = {"wifi_delete_q", "Delete the connection to %s?"},
    [PW_S_CAL_TITLE] = {"cal_title", "Battery Calibration"},
    [PW_S_CAL_S1] = {"cal_s1", "Charge"},
    [PW_S_CAL_S2] = {"cal_s2", "Unplug"},
    [PW_S_CAL_S3] = {"cal_s3", "Run down"},
    [PW_S_CAL_S4] = {"cal_s4", "Result"},
    [PW_S_CAL_HOW] = {"cal_how",
                      "Measures what your battery really holds: charge it full, unplug the charger "
                      "and let the PSP run down with Home on. It takes a few hours."},
    [PW_S_CAL_DO_CHARGE] = {"cal_do_charge",
                            "Connect the charger and leave it until the battery is full."},
    [PW_S_CAL_DO_UNPLUG] = {"cal_do_unplug",
                            "Full. Unplug the charger: the measurement starts by itself."},
    [PW_S_CAL_DO_RUN] = {"cal_do_run", "Measuring. Leave Home on until the PSP turns itself off, "
                                       "then charge it and open this page again."},
    [PW_S_CAL_DO_DONE] = {"cal_do_done", "Measured. Start again for a new measurement."},
    [PW_S_CAL_ELAPSED] = {"cal_elapsed", "Elapsed"},
    [PW_S_CAL_USED] = {"cal_used", "Used"},
    [PW_S_CAL_AVERAGE] = {"cal_average", "Average"},
    [PW_S_CAL_PROJECTED] = {"cal_projected", "Full charge"},
    [PW_S_CAL_LATER] = {"cal_later", "Later"},
    [PW_S_CAL_NO_CHART] = {"cal_no_chart", "The chart fills in while it measures"},
    [PW_S_CAL_STOP] = {"cal_stop", "Stop"},
    [PW_S_CAL_STOP_Q] = {"cal_stop_q",
                         "Stop the measurement? It is kept only if it ran long enough."},
    [PW_S_HS_OSD] = {"hs_osd", "Volume and Brightness"},
    [PW_S_HS_OSD_HOME] = {"hs_osd_home", "In Home"},
    [PW_S_HS_OSD_ALL] = {"hs_osd_all", "Home and Games"},
    [PW_S_HS_BOOST] = {"hs_boost", "Brightness Boost"},
    [PW_S_HS_BOOST_HALF] = {"hs_boost_half", "Brighter"},
    [PW_S_HS_BOOST_MAX] = {"hs_boost_max", "Maximum"},
    [PW_S_OSD_MUTED] = {"osd_muted", "Muted"},
#endif
};

static const char *g_text[PW_I18N_HELD]; /* NULL: English */
static char g_pool[PW_I18N_POOL];        /* the translated texts (kernel RAM: part of a state) */

const char *pw_tr(int id)
{
    if (id < 0 || id >= PW_I18N_HELD) /* another build's string */
        return "";
    return g_text[id] ? g_text[id] : g_english[id].text ? g_english[id].text : "";
}

const char *pw_i18n_key(int id)
{
    return id >= 0 && id < PW_I18N_HELD ? g_english[id].key : 0;
}

const char *pw_i18n_english(int id)
{
    return id >= 0 && id < PW_I18N_HELD ? g_english[id].text : 0;
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
    for (int i = 0; i < PW_I18N_HELD; i++)
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
        for (int i = 0; i < PW_I18N_HELD; i++)
            if (g_text[i] && !covers(g_text[i], ctx)) {
                pw_i18n_reset();
                return -2;
            }
    return taken;
}
