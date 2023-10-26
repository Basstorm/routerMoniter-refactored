#include "data_source.h"

#include "ui.h"
#include "config.h"
#include "utils.h"

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <WiFiCLient.h>
#include <ESP8266HTTPClient.h>

static unsigned long last_run_milsec = 0;

void get_netdata_chart_info() {
    unsigned long now = millis();
    if (now - last_run_milsec < 1970) {
        return;
    }
    WiFiClient wifi_client;
    HTTPClient httpClient;
    DynamicJsonDocument doc(1024);
    String chart_ids[4] = {"system.cpu", "mem.available", "net.eth0", "sensors.temp_thermal_zone0_thermal_thermal_zone0"};
    for(int i = 0; i < 4; i++) {
        String s_url = "http://" + String(ROUTER_ADDR) + ":" + String(NETDATA_PORT) + "/api/v1/data?chart=" + chart_ids[i] + "&format=array&points=9&group=average&gtime=0&options=s%7Cjsonwrap%7Cnonzero&after=-2";
        httpClient.begin(wifi_client, s_url.c_str());
        int httpCode = httpClient.GET();
        if (httpCode!= HTTP_CODE_OK) {
            LOG("HTTP GET request failed with code: %d\n", httpCode);
            continue;
        }
        
        String response = httpClient.getString().c_str();
        DeserializationError error = deserializeJson(doc, response);
        if (error != nullptr) {
            LOG("Error : %s, response: %s\n", error.c_str(), response);
            return;
        }
        String dimension_name = doc["dimension_names"][0];
        double latest_value = doc["latest_values"][0];
        if (dimension_name.equals(F("temp"))) {
            set_temperature(latest_value);
        } else if(dimension_name.equals(F("softirq"))) {
            set_cpu_usage(latest_value);
        } else if(dimension_name.equals(F("avail"))) {
            set_mem_usage(latest_value);
        } else if(dimension_name.equals(F("received"))) {
            double up_speed = doc["latest_values"][1];
            double down_speed = doc["latest_values"][0];
            set_network_speed(up_speed, down_speed);
        }
        httpClient.end();
    }

    last_run_milsec = millis();
}