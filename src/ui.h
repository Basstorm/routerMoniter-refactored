#ifndef _UI_H_
#define _UI_H_
#include <lvgl.h>

enum {
    SCREEN_PAGE_NULL = 0,
    SCREEN_PAGE_WIFI_LOGIN,
    SCREEN_PAGE_MONITOR,
    SCREEN_PAGE_MAX = SCREEN_PAGE_MONITOR
};

uint8_t get_current_page();
void set_ip_addr(const char *ip_addr);
void set_cpu_usage(double cpu_usage);
void set_mem_usage(double mem_usage);
void set_temperature(double temp_value);
void set_network_speed(double up, double down);
void switch_to_monitor_ui();
void switch_to_login_ui();
void setup_ui();

#endif
