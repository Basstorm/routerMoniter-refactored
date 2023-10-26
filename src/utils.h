#ifndef __UTILS_H
#define __UTILS_H

#include <cstdint>
#include <ESP8266WiFi.h>

#if defined(DEBUG) || defined(__DEBUG__)
#define LOG(...) Serial.printf(__VA_ARGS__);
#else
#define LOG(...)
#endif

double bytes_to_human_size(double bytes, char **human_size);

#endif