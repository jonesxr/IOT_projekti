#pragma once
#include <Arduino.h>

void initColors();
void drawWifiScreen(const char* status, int progress, uint16_t color);
void drawHomeScreen(float lux = -1.0f);
void updateVUMeter(int volume);
void utf8toAscii(char* dst, const char* src, int maxLen);
