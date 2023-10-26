#include "tft.h"
#include "ui.h"
#include "data_source.h"
#include "config.h"
#include "utils.h"

#include <string>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define SECS_PER_HOUR 3600

static WiFiEventHandler onStationModeConnectedHandler;
static WiFiEventHandler onStationModeDisconnectedHandler;
static WiFiEventHandler onStationModeGotIPHandler;

static WiFiUDP ntp_udp;
static NTPClient ntp_client(ntp_udp, NTP_SERVER, NTP_TIMEZONE * SECS_PER_HOUR, NTP_SYNC_INTERVAL);

static void on_station_mode_connected_handler(const WiFiEventStationModeConnected &event) {
    LOG("[%s] WiFi Connected.\n", ntp_client.getFormattedTime());
    switch_to_monitor_ui();
}

static void on_station_mode_disconnected_handler(const WiFiEventStationModeDisconnected &event) {
    LOG("[%s] WiFi Disconnected.\n", ntp_client.getFormattedTime());
    switch_to_login_ui();
}

static void on_station_mode_got_ip_handler(const WiFiEventStationModeGotIP &event) {
    LOG("[%s] WiFi Got IP: %s.\n", ntp_client.getFormattedTime(), WiFi.localIP().toString().c_str());
    set_ip_addr(WiFi.localIP().toString().c_str());
    ntp_client.begin();
}

void setup() {
    Serial.begin(921600); /* prepare for possible serial debug */
    srand((unsigned)time(NULL));
    
    // 初始化屏幕
    init_tft_screen();

    // 初始化ui
    setup_ui();

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    onStationModeConnectedHandler = WiFi.onStationModeConnected(on_station_mode_connected_handler);
    onStationModeDisconnectedHandler = WiFi.onStationModeDisconnected(on_station_mode_disconnected_handler);
    onStationModeGotIPHandler = WiFi.onStationModeGotIP(on_station_mode_got_ip_handler);
}

void loop() {
    uint32_t delay_interval = lv_task_handler(); /* let the GUI do its work */
    if (WiFi.isConnected()) {
        get_netdata_chart_info();
    }
    ntp_client.update();
    automatic_brightness(ntp_client);
    delay(delay_interval);
}
