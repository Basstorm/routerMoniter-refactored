#include "data_source.h"

#include "ui.h"
#include "config.h"
#include "utils.h"

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <WiFiCLient.h>
#include <ESP8266HTTPClient.h>

enum {
    DATA_SOURCE_CPU_USAGE = 0,
    DATA_SOURCE_MEM_USAGE,
    DATA_SOURCE_NETWORK,
    DATA_SOURCE_TEMP,
    // add new data source below this comment
    DATA_SOURCE_MAX
};

static unsigned long last_run_milsec = 0;

void get_netdata_chart_info() {
    unsigned long now = millis();
    if (now - last_run_milsec < 1970) {
        return;
    }
    WiFiClient wifi_client;
    HTTPClient http_client;
    DynamicJsonDocument doc(1024);
    String chart_ids[DATA_SOURCE_MAX] = {
        F("system.cpu"), 
        F("mem.available"), 
        F("net.eth0"), 
        F("sensors.temp_thermal_zone0_thermal_thermal_zone0")
    };

    for(int i = 0; i < DATA_SOURCE_MAX; i++) {
        String request_url = F("http://") + String(ROUTER_ADDR) + F(":") + String(NETDATA_PORT) + F("/api/v1/data?chart=") + chart_ids[i] + F("&format=array&points=9&group=average&gtime=0&options=s%7Cjsonwrap%7Cnonzero&after=-2");

        http_client.begin(wifi_client, request_url.c_str());
        int http_code = http_client.GET();

        if (http_code != HTTP_CODE_OK) {
            LOG("HTTP GET request failed with code: %d\n", http_code);
            continue;
        }
        DeserializationError error = ArduinoJson::deserializeJson(doc, http_client.getString().c_str());
        if (error != nullptr) {
            LOG("Error : %s, response: %s\n", error.c_str(), http_client.getString().c_str());
            return;
        }
        if (doc.containsKey(F("latest_values")) == false) {
            LOG("Error: json result doesnt contain latest_values key.\n");
            return;
        }
         
        JsonArray latest_values = doc[F("latest_values")];
        switch (i) {
            case DATA_SOURCE_CPU_USAGE:
                set_cpu_usage(latest_values[0]);
                break;
            case DATA_SOURCE_MEM_USAGE:
                set_mem_usage(latest_values[0]);
                break;
            case DATA_SOURCE_NETWORK:
                set_network_speed(latest_values[1], latest_values[0]);
                break;
            case DATA_SOURCE_TEMP:
                set_temperature(latest_values[0]);
                break;
            default:
                break;
        }
        doc.clear();
        http_client.end();
    }

    last_run_milsec = millis();
}