// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <psppower.h>

#include "core/config.h"
#include "gfx/assets.h"
#include "ui/ui.h"

#define DPAD (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)

int sceImposeGetLanguageMode(int *lang, int *button); /* core/stubs/sceImpose_driver.S */

unsigned int pw_ui_now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

unsigned int pw_ui_buttons(void)
{
    SceCtrlData pad;

    return sceCtrlPeekBufferPositive(&pad, 1) < 0 ? 0 : pad.Buttons;
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
    if (sceImposeGetLanguageMode(&lang, &button) < 0)
        button = 1;
    ui->cross_enter = button != 0;
    ui->enter = ui->cross_enter ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
    ui->back = ui->cross_enter ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
    ui->t_tick = 0;
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
    return pw_keys_update(&ui->keys, pw_ui_buttons(), DPAD, sceKernelGetSystemTimeLow());
}
