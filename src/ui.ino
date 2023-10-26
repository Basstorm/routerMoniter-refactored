#include "ui.h"

#include "tft.h"
#include "utils.h"

#include <ESP8266WiFi.h>

// 监测值
lv_coord_t upload_series[10] = {0};
lv_coord_t download_series[10] = {0};
lv_coord_t up_speed_max = 0;
lv_coord_t down_speed_max = 0;

// ----------------------------------------------- 页面 ------------------------------------
LV_FONT_DECLARE(tencent_w7_22)
LV_FONT_DECLARE(tencent_w7_24)

static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];


static lv_obj_t *login_page = NULL;
static lv_obj_t *monitor_page = NULL;

// basic variables
static lv_style_t arc_indic_style;
static lv_obj_t *up_speed_label = NULL;
static lv_obj_t *up_speed_unit_label = NULL;
static lv_obj_t *down_speed_label = NULL;
static lv_obj_t *down_speed_unit_label = NULL;
static lv_obj_t *cpu_bar = NULL;
static lv_obj_t *cpu_value_label = NULL;
static lv_obj_t *mem_bar = NULL;
static lv_obj_t *mem_value_label = NULL;
static lv_obj_t *temp_value_label = NULL;
static lv_obj_t *temperature_arc = NULL;
static lv_obj_t *ip_label = NULL;
static lv_obj_t *chart = NULL;

static lv_chart_series_t *ser1 = NULL;
static lv_chart_series_t *ser2 = NULL;

static uint8_t current_page = SCREEN_PAGE_NULL;
// ----------------------------------------------- 页面 ------------------------------------

static void ui_task_cb(lv_task_t *task) {
    if (chart != NULL) lv_chart_refresh(chart);
    LOG("⚠ Left Memory: %u\n", ESP.getFreeHeap());
}

/* Reading input device (simulated encoder here) */
static bool read_encoder(lv_indev_drv_t *indev, lv_indev_data_t *data)
{
    static int32_t last_diff = 0;
    int32_t diff = 0;                   /* Dummy - no movement */
    int btn_state = LV_INDEV_STATE_REL; /* Dummy - no press */

    data->enc_diff = diff - last_diff;
    data->state = btn_state;

    last_diff = diff;

    return false;
}

static lv_coord_t update_net_series(lv_coord_t *series, double speed) {
    lv_coord_t local_max = series[0];
    for (int index = 0; index < 9; index++) {
        series[index] = series[index + 1];
        if (local_max < series[index]) {
            local_max = series[index];
        }
    }
    series[9] = (lv_coord_t)speed;
    if (local_max < series[9]) local_max = series[9];

    return local_max;
}

uint8_t get_current_page() {
    return current_page;
}

void set_ip_addr(const char *ip_addr) {
    if (ip_label != NULL) lv_label_set_text(ip_label, ip_addr);
}

void set_cpu_usage(double cpu_usage) {
    if (cpu_bar != NULL) lv_bar_set_value(cpu_bar, cpu_usage, LV_ANIM_ON);
    if (cpu_value_label != NULL) lv_label_set_text_fmt(cpu_value_label, "%2.1f%%", cpu_usage);
}

void set_mem_usage(double mem_usage) {
    double mem = 100 * (1.0 - mem_usage / 1024.0);
    if (mem_bar != NULL) lv_bar_set_value(mem_bar, mem, LV_ANIM_ON);
    if (mem_value_label != NULL) lv_label_set_text_fmt(mem_value_label, "%2.0f%%", mem);
}

void set_temperature(double temp_value) {
    if (temp_value_label != NULL) lv_label_set_text_fmt(temp_value_label, "%2.0f°C", temp_value);
    uint16_t end_value = 120 + 300 * temp_value / 100.0f;
    lv_color_t arc_color = temp_value > 75 ? lv_color_hex(0xff5d18) : lv_color_hex(0x50ff7d);
    lv_style_set_line_color(&arc_indic_style, LV_STATE_DEFAULT, arc_color);
    if (temperature_arc != NULL) {
        lv_obj_add_style(temperature_arc, LV_ARC_PART_INDIC, &arc_indic_style);
        lv_arc_set_end_angle(temperature_arc, end_value);
    }
}

void set_network_speed(double up, double down) {
    double up_speed = -1 * up / 8.0;
    up_speed_max = update_net_series(upload_series, up_speed);
    double down_speed = down / 8.0; // byte = 8 bit
    down_speed_max = update_net_series(download_series, down_speed);
    lv_coord_t max_speed = max(down_speed_max, up_speed_max);
    max_speed = max(max_speed, (lv_coord_t)16);

    if (chart != NULL && ser1 != NULL && ser2 != NULL) {
        lv_chart_set_points(chart, ser1, upload_series);
        lv_chart_set_points(chart, ser2, download_series);
        lv_chart_set_range(chart, 0, (lv_coord_t)(max_speed * 1.1));
    }

    char *human_lable = NULL;
    double show_up_speed = bytes_to_human_size(up_speed, &human_lable);
    if (up_speed_label != NULL) lv_label_set_text_fmt(up_speed_label, show_up_speed > 100.0 ? "%.1f" : "%.2f", show_up_speed);
    if (up_speed_unit_label != NULL) lv_label_set_text(up_speed_unit_label, human_lable);

    double show_down_speed = bytes_to_human_size(down_speed, &human_lable);
    if (down_speed_label != NULL) lv_label_set_text_fmt(down_speed_label, show_down_speed > 100.0 ? "%.1f" : "%.2f", show_down_speed);
    if (down_speed_unit_label != NULL) lv_label_set_text(down_speed_unit_label, human_lable);    
}

static void create_monitor_ui() {
    if (monitor_page == NULL) {
        monitor_page = lv_cont_create(lv_scr_act(), NULL);
        lv_obj_set_size(monitor_page, 240, 240);
        lv_obj_set_hidden(monitor_page, true);

        lv_obj_t *bg;
        bg = lv_obj_create(monitor_page, NULL);
        lv_obj_clean_style_list(bg, LV_OBJ_PART_MAIN);
        lv_obj_set_style_local_bg_opa(bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_100);
        lv_color_t bg_color = lv_color_hex(0x7381a2);
        // bg_color = lv_color_hex(0xecdd5c);
        lv_obj_set_style_local_bg_color(bg, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, bg_color);
        lv_obj_set_size(bg, LV_HOR_RES_MAX, LV_VER_RES_MAX);

        // 显示ip地址
        ip_label = lv_label_create(monitor_page, NULL);
        //lv_label_set_text(ip_label, WiFi.localIP().toString().c_str());
        lv_label_set_text(ip_label, "No IP NOW");
        lv_obj_set_pos(ip_label, 10, 220);

        lv_obj_t *cont = lv_cont_create(monitor_page, NULL);
        lv_obj_set_auto_realign(cont, true); /*Auto realign when the size changes*/
        // lv_obj_align_origo(cont, NULL, LV_ALIGN_IN_TOP_LEFT, 120, 35);  /*This parametrs will be sued when realigned*/
        // lv_color_t cont_color = lv_color_hex(0x1a1d25);
        lv_color_t cont_color = lv_color_hex(0x081418);
        lv_obj_set_width(cont, 230);
        lv_obj_set_height(cont, 120);
        lv_obj_set_pos(cont, 5, 5);

        lv_cont_set_fit(cont, LV_FIT_TIGHT);
        lv_cont_set_layout(cont, LV_LAYOUT_COLUMN_MID);
        lv_obj_set_style_local_border_color(cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, cont_color);
        lv_obj_set_style_local_bg_color(cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, cont_color);

        // Upload & Download Symbol
        static lv_style_t iconfont;
        lv_style_init(&iconfont);
        lv_style_set_text_font(&iconfont, LV_STATE_DEFAULT, &iconfont_symbol);

        lv_obj_t *upload_label = lv_label_create(monitor_page, NULL);
        lv_obj_add_style(upload_label, LV_LABEL_PART_MAIN, &iconfont);
        lv_label_set_text(upload_label, CUSTOM_SYMBOL_UPLOAD);
        lv_color_t speed_label_color = lv_color_hex(0x838a99);
        lv_obj_set_style_local_text_color(upload_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_RED);
        lv_obj_set_pos(upload_label, 10, 18);

        lv_obj_t *down_label = lv_label_create(monitor_page, NULL);
        lv_obj_add_style(down_label, LV_LABEL_PART_MAIN, &iconfont);
        lv_label_set_text(down_label, CUSTOM_SYMBOL_DOWNLOAD);
        speed_label_color = lv_color_hex(0x838a99);
        lv_obj_set_style_local_text_color(down_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GREEN);
        lv_obj_set_pos(down_label, 120, 18);

        // Upload & Download Speed Display
        static lv_style_t font_22;
        lv_style_init(&font_22);
        // lv_style_set_text_font(&font_22, LV_STATE_DEFAULT, &lv_font_montserrat_24);
        lv_style_set_text_font(&font_22, LV_STATE_DEFAULT, &tencent_w7_22);

        up_speed_label = lv_label_create(monitor_page, NULL);
        lv_label_set_text(up_speed_label, "56.78");
        lv_obj_add_style(up_speed_label, LV_LABEL_PART_MAIN, &font_22);
        lv_obj_set_style_local_text_color(up_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_obj_set_pos(up_speed_label, 30, 15);

        up_speed_unit_label = lv_label_create(monitor_page, NULL);
        lv_label_set_text(up_speed_unit_label, "K/S");
        lv_obj_set_style_local_text_color(up_speed_unit_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, speed_label_color);
        lv_obj_set_pos(up_speed_unit_label, 90, 18);

        down_speed_label = lv_label_create(monitor_page, NULL);
        lv_label_set_text(down_speed_label, "12.34");
        lv_obj_add_style(down_speed_label, LV_LABEL_PART_MAIN, &font_22);
        lv_obj_set_style_local_text_color(down_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_obj_set_pos(down_speed_label, 142, 15);

        down_speed_unit_label = lv_label_create(monitor_page, NULL);
        lv_label_set_text(down_speed_unit_label, "M/S");
        lv_obj_set_style_local_text_color(down_speed_unit_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, speed_label_color);
        lv_obj_set_pos(down_speed_unit_label, 202, 18);

        // 绘制曲线图
        /*Create a chart*/
        chart = lv_chart_create(monitor_page, NULL);
        lv_obj_set_size(chart, 220, 70);
        lv_obj_align(chart, NULL, LV_ALIGN_CENTER, 0, -40);
        lv_chart_set_type(chart, LV_CHART_TYPE_LINE); /*Show lines and points too*/
        lv_chart_set_range(chart, 0, 4096);
        lv_chart_set_point_count(chart, 10); // 设置显示点数
        lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

        /*Add a faded are effect*/
        lv_obj_set_style_local_bg_opa(chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, LV_OPA_50); /*Max. opa.*/
        lv_obj_set_style_local_bg_grad_dir(chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, LV_GRAD_DIR_VER);
        lv_obj_set_style_local_bg_main_stop(chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 255); /*Max opa on the top*/
        lv_obj_set_style_local_bg_grad_stop(chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 0);   /*Transparent on the bottom*/

        /*Add two data series*/
        ser1 = lv_chart_add_series(chart, LV_COLOR_RED);
        ser2 = lv_chart_add_series(chart, LV_COLOR_GREEN);

        // /*Directly set points on 'ser2'*/
        lv_chart_set_points(chart, ser2, download_series);
        lv_chart_set_points(chart, ser1, upload_series);

        lv_chart_refresh(chart); /*Required after direct set*/

        // 绘制进度条  CPU 占用
        lv_obj_t *cpu_title = lv_label_create(monitor_page, NULL);
        lv_label_set_text(cpu_title, "CPU");
        lv_obj_set_style_local_text_color(cpu_title, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_obj_set_pos(cpu_title, 5, 140);

        cpu_value_label = lv_label_create(monitor_page, NULL);
        lv_label_set_text(cpu_value_label, "34%");
        lv_obj_add_style(cpu_value_label, LV_LABEL_PART_MAIN, &font_22);
        lv_obj_set_style_local_text_color(cpu_value_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_obj_set_pos(cpu_value_label, 85, 135);

        lv_color_t cpu_bar_indic_color = lv_color_hex(0x63d0fc);
        lv_color_t cpu_bar_bg_color = lv_color_hex(0x1e3644);
        cpu_bar = lv_bar_create(monitor_page, NULL);
        lv_obj_set_size(cpu_bar, 130, 10);
        lv_obj_set_pos(cpu_bar, 5, 160);

        lv_obj_set_style_local_bg_color(cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, cpu_bar_bg_color);
        lv_obj_set_style_local_bg_color(cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, cpu_bar_indic_color);
        lv_obj_set_style_local_border_width(cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 2);
        lv_obj_set_style_local_border_width(cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, 2);

        lv_obj_set_style_local_border_color(cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, cont_color);
        lv_obj_set_style_local_border_color(cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, cont_color);
        lv_obj_set_style_local_border_side(cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM);
        lv_obj_set_style_local_radius(cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 2);
        lv_obj_set_style_local_radius(cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, 0);

        // 绘制内存占用
        lv_obj_t *men_title = lv_label_create(monitor_page, NULL);
        lv_label_set_text(men_title, "Memory");
        lv_obj_set_style_local_text_color(men_title, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_obj_set_pos(men_title, 5, 180);

        mem_value_label = lv_label_create(monitor_page, NULL);
        lv_label_set_text(mem_value_label, "42%");
        lv_obj_add_style(mem_value_label, LV_LABEL_PART_MAIN, &font_22);
        lv_obj_set_style_local_text_color(mem_value_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_obj_set_pos(mem_value_label, 85, 175);

        mem_bar = lv_bar_create(monitor_page, NULL);
        lv_obj_set_size(mem_bar, 130, 10);
        lv_obj_set_pos(mem_bar, 5, 200);
        lv_obj_set_style_local_bg_color(mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, cpu_bar_bg_color);
        lv_obj_set_style_local_bg_color(mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, cpu_bar_indic_color);
        lv_obj_set_style_local_border_width(mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 2);
        lv_obj_set_style_local_border_color(mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, cont_color);
        lv_obj_set_style_local_border_width(mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, 2);
        lv_obj_set_style_local_border_color(mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, cont_color);
        lv_obj_set_style_local_border_side(mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM);
        lv_obj_set_style_local_radius(mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 2);
        lv_obj_set_style_local_radius(mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, 0);

        // 绘制温度表盘
        static lv_style_t arc_style;
        lv_style_reset(&arc_style);
        lv_style_init(&arc_style);
        lv_style_set_bg_opa(&arc_style, LV_STATE_DEFAULT, LV_OPA_TRANSP);
        lv_style_set_border_opa(&arc_style, LV_STATE_DEFAULT, LV_OPA_TRANSP);
        lv_style_set_line_width(&arc_style, LV_STATE_DEFAULT, 100);
        lv_style_set_line_color(&arc_style, LV_STATE_DEFAULT, lv_color_hex(0x081418));
        lv_style_set_line_rounded(&arc_style, LV_STATE_DEFAULT, false);

        lv_style_init(&arc_indic_style);
        lv_style_set_line_width(&arc_indic_style, LV_STATE_DEFAULT, 5);
        lv_style_set_pad_left(&arc_indic_style, LV_STATE_DEFAULT, 5);
        // lv_style_set_line_color(&arc_indic_style, LV_STATE_DEFAULT, lv_color_hex(0x50ff7d));
        lv_style_set_line_color(&arc_indic_style, LV_STATE_DEFAULT, lv_color_hex(0xff5d18));
        // lv_style_set_line_rounded(&arc_indic_style, LV_STATE_DEFAULT, false);

        temperature_arc = lv_arc_create(monitor_page, NULL);
        lv_arc_set_bg_angles(temperature_arc, 0, 360);
        lv_arc_set_start_angle(temperature_arc, 120);
        lv_arc_set_end_angle(temperature_arc, 420);
        lv_obj_set_size(temperature_arc, 125, 125);
        lv_obj_set_pos(temperature_arc, 125, 120);
        lv_obj_add_style(temperature_arc, LV_ARC_PART_BG, &arc_style);
        lv_obj_add_style(temperature_arc, LV_ARC_PART_INDIC, &arc_indic_style);
        // lv_obj_align(temperature_arc, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 10, 10);

        static lv_style_t font_24;
        lv_style_init(&font_24);
        lv_style_set_text_font(&font_24, LV_STATE_DEFAULT, &tencent_w7_24);

        temp_value_label = lv_label_create(monitor_page, NULL);
        lv_label_set_text(temp_value_label, "72℃");
        lv_obj_add_style(temp_value_label, LV_LABEL_PART_MAIN, &font_24);
        lv_obj_set_style_local_text_color(temp_value_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
        lv_obj_set_pos(temp_value_label, 160, 170);
    }
}

void destroy_login_page() {
    if (login_page != NULL) {
        lv_obj_del(login_page);
        lv_obj_set_hidden(login_page, true);
        login_page = NULL;
    } 
}

void switch_to_monitor_ui() {
    uint8_t page = get_current_page();
    switch (page) {
        case SCREEN_PAGE_NULL : {
            break;
        }
        case SCREEN_PAGE_WIFI_LOGIN: {
            destroy_login_page();
            create_monitor_ui();
            lv_obj_set_hidden(monitor_page, false);
            current_page = SCREEN_PAGE_MONITOR;
            break;
        }
        case SCREEN_PAGE_MONITOR: {
            break;
        }
    }
}

static void create_login_ui() {
    if (login_page == NULL) {
        login_page = lv_cont_create(lv_scr_act(), NULL);
        lv_obj_set_hidden(login_page, true);
        lv_obj_set_size(login_page, 240, 240); // 设置容器大小
        lv_obj_set_style_local_bg_color(login_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
        lv_obj_set_style_local_border_color(login_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
        lv_obj_set_style_local_radius(login_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

        static lv_style_t login_spinner_style;
        lv_style_init(&login_spinner_style);
        lv_style_set_line_width(&login_spinner_style, LV_STATE_DEFAULT, 5);
        lv_style_set_pad_left(&login_spinner_style, LV_STATE_DEFAULT, 5);
        lv_style_set_line_color(&login_spinner_style, LV_STATE_DEFAULT, lv_color_hex(0xff5d18));

        lv_obj_t *preload = lv_spinner_create(login_page, NULL);
        lv_obj_set_size(preload, 100, 100);
        lv_obj_align(preload, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_style(preload, LV_OBJ_PART_MAIN, &login_spinner_style);
    }
}

void destroy_monitor_page() {
    if (monitor_page != NULL) {
        lv_obj_set_hidden(monitor_page, true);
        lv_obj_del(monitor_page);
        monitor_page = NULL;
        up_speed_label = NULL;
        up_speed_unit_label = NULL;
        down_speed_label = NULL;
        down_speed_unit_label = NULL;
        cpu_bar = NULL;
        cpu_value_label = NULL;
        mem_bar = NULL;
        mem_value_label = NULL;
        temp_value_label = NULL;
        temperature_arc = NULL;
        ip_label = NULL;
        chart = NULL;
        ser1 = NULL;
        ser2 = NULL;
    }
}

void switch_to_login_ui() {
    uint8_t page = get_current_page();
    switch (page) {
        case SCREEN_PAGE_NULL : {
            create_login_ui();
            lv_obj_set_hidden(login_page, false);
            current_page = SCREEN_PAGE_WIFI_LOGIN;
            break;
        }
        case SCREEN_PAGE_WIFI_LOGIN: {
            break;
        }
        case SCREEN_PAGE_MONITOR: {
            create_login_ui();
            lv_obj_set_hidden(login_page, false);
            if (monitor_page != NULL) {
                lv_obj_set_hidden(monitor_page, true);
                lv_obj_del(monitor_page);
                monitor_page = NULL;
            }
            current_page = SCREEN_PAGE_WIFI_LOGIN;
            break;
        }
    }
}

void setup_ui() {
    lv_init();
    lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);

    /*Initialize the display*/
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = disp_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the (dummy) input device driver*/
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = read_encoder;
    lv_indev_drv_register(&indev_drv);

    switch_to_login_ui();
    lv_task_create(ui_task_cb, 2000, LV_TASK_PRIO_MID, NULL);
}