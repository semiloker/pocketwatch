// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <psppower.h>

#include "core/config.h"
#include "core/platform.h"
#include "gfx/assets.h"
#include "ui/fb.h"
#include "ui/ui.h"
#include "ui/views.h"

#define DPAD     (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)
#define VOL_KEYS (PSP_CTRL_VOLUP | PSP_CTRL_VOLDOWN)

/* core/stubs/sceImpose_driver.S; the parameter numbers are PSPSDK's pspimpose_driver.h */
int sceImposeGetLanguageMode(int *lang, int *button);
int sceImposeGetParam(int param);
int sceDisplayGetBrightness(int *level, int *unk); /* pspdisplay_driver */
#define IMPOSE_MAIN_VOLUME 0x1
#define IMPOSE_MUTE        0x8

/* The system's volume and mute now into the bar; returns 1 when the screen needs a frame. */
static int volume_tick(pw_ui *ui, unsigned int buttons)
{
    unsigned int vol = buttons & VOL_KEYS;
    int level = sceImposeGetParam(IMPOSE_MAIN_VOLUME), muted = sceImposeGetParam(IMPOSE_MUTE),
        pressed = (vol & ~ui->vol_held) != 0;

    ui->vol_held = vol;
    if (level < 0)
        return 0;
    return pw_volume_update(&ui->volume, level, muted > 0, pressed, pw_ui_now_ms());
}

/* The step of the backlight's level (0-100) into the brightness bar: the level changes at each of
   the SCREEN button's steps, the fifth too (impose's step does not, #6), the bar fills by the step
   (#11); the screen off (0) is no change. */
static int bright_tick(pw_ui *ui)
{
    int level = 0, unk = 0, step;

    if (sceDisplayGetBrightness(&level, &unk) < 0 || level <= 0) {
        if (!ui->bright.known)
            return 0;
        step = ui->bright.level;
    } else {
        step = pw_backlight_step(level);
    }
    return pw_volume_update(&ui->bright, step, 0, 0, pw_ui_now_ms());
}

/* Over every frame just before it is shown (pw_fb_present), so every screen has the bars. */
static void draw_overlay(pw_canvas *c, void *ctx)
{
    pw_ui *ui = ctx;

    volume_tick(ui, pw_ui_buttons());
    bright_tick(ui);
    if (ui->volume.alpha > 0)
        pw_view_volume(c, ui, &ui->volume);
    if (ui->bright.alpha > 0)
        pw_view_volume(c, ui, &ui->bright);
}

unsigned int pw_ui_now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

unsigned int pw_ui_now_us(void)
{
    return sceKernelGetSystemTimeLow();
}

unsigned int pw_ui_buttons(void)
{
    return pw_buttons_held(); /* ARK-4's exit combos stay off while the menu runs (SHL-REQ-012) */
}

void pw_ui_init(pw_ui *ui)
{
    int lang = 1, button = 1;

    ui->title = pw_assets_font(PW_FONT_TITLE);
    ui->style = pw_config_get()->style;
    ui->body = pw_assets_font(PW_FONT_BODY);
    ui->btn[0] = pw_assets_art(PW_ART_BTN_CROSS);
    ui->btn[1] = pw_assets_art(PW_ART_BTN_CIRCLE);
    ui->btn[2] = pw_assets_art(PW_ART_BTN_TRIANGLE);
    ui->btn[3] = pw_assets_art(PW_ART_BTN_SQUARE);
    ui->battery = pw_assets_art(PW_ART_BATTERY);
    ui->focus = pw_assets_art(PW_ART_FOCUS);
    if (sceImposeGetLanguageMode(&lang, &button) < 0)
        button = 1;
    ui->cross_enter = button != 0;
    ui->enter = ui->cross_enter ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
    ui->back = ui->cross_enter ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
    ui->t_tick = 0;
    pw_volume_reset(&ui->volume);
    pw_volume_reset(&ui->bright);
    ui->volume.max = 0; /* (PW_VOLUME_MAX) */
    ui->bright.max = PW_BRIGHT_STEPS;
    ui->vol_held = pw_ui_buttons() & VOL_KEYS;
    pw_fb_set_overlay(draw_overlay, ui);
    pw_ui_flush_input(ui);
}

void pw_ui_flush_input(pw_ui *ui)
{
    ui->keys.prev = pw_ui_buttons();
    ui->keys.t_press = ui->keys.t_repeat = sceKernelGetSystemTimeLow();
}

unsigned int pw_ui_poll(pw_ui *ui)
{
    unsigned int now;

    sceDisplayWaitVblankStart();
    now = pw_ui_now_ms();
    if (now - ui->t_tick > 1000) { /* keep the backlight on */
        scePowerTick(0);
        ui->t_tick = now;
    }
    {
        unsigned int buttons = pw_ui_buttons(),
                     ev = pw_keys_update(&ui->keys, buttons, DPAD, sceKernelGetSystemTimeLow());

        if (volume_tick(ui, buttons) | bright_tick(ui))
            ev |= PW_UI_EV_FRAME;

#ifdef PW_DEBUG
        if (ev & PSP_CTRL_SELECT) { /* debug builds: SELECT photographs the menu for the docs */
            pw_fb_screenshot();
            ev &= ~(unsigned int)PSP_CTRL_SELECT;
        }
#endif
        return ev;
    }
}
