#include "tft.h"

#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// void show_web_page() {
//     TFT_eSprite clk = TFT_eSprite(&tft);

//     clk.setColorDepth(8);

//     clk.createSprite(200, 60); //创建窗口
//     clk.fillSprite(0x0000);    //填充率

//     clk.setTextDatum(CC_DATUM); //设置文本数据
//     clk.setTextColor(TFT_GREEN, 0x0000);
//     clk.drawString("WiFi Connect Fail!", 100, 10, 2);
//     clk.drawString("SSID:", 45, 40, 2);
//     clk.setTextColor(TFT_WHITE, 0x0000);
//     clk.drawString("routerMonitor", 125, 40, 2);
//     clk.pushSprite(20, 50); //窗口位置

//     clk.deleteSprite();
// }

/* Display flushing */
void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// 屏幕亮度设置，value [0, 256] 越小越亮,越大越暗
void set_brightness(int value) {
    pinMode(TFT_BL, INPUT);
    analogWrite(TFT_BL, value);
    pinMode(TFT_BL, OUTPUT);
}

static unsigned long last_run_auto_brightness_milsec = 0;
void automatic_brightness(NTPClient &ntp_client) {
    unsigned long now_mil = millis();
    if (now_mil - last_run_auto_brightness_milsec < 10000) return;

    int hours = ntp_client.getHours();
    // night 20:00 - 8:00
    if ((hours >= 20 && hours <= 24) || (hours >= 0 && hours <= 8)) {
        set_brightness(240);
    } else {
        set_brightness(180);
    }
    
    last_run_auto_brightness_milsec = millis();
}

// 屏幕初始化
void init_tft_screen() {
    tft.begin();        /* TFT init */
    tft.setRotation(0); /* Landscape orientation */
    set_brightness(220);
}
