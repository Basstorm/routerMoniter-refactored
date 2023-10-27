#ifndef __TFT_H
#define __TFT_H

#include "ui.h"

#include <NTPClient.h>

void show_web_page();
void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void init_tft_screen();
void set_brightness(int value);
void automatic_brightness(NTPClient &ntp_client);

#endif