#pragma once
#include <Arduino.h>

void initColors();
void drawWifiScreen(const char* status, int progress, uint16_t color);
void drawHomeScreen(float lux = -1.0f);
void drawSensorScreen(float lux = -1.0f, int volume = 0, int mqLevel = 0);
void drawInfoScreen();
void updateVUMeter(int volume);
void updateSensorVUMeter(int volume);
void updateMQBar(int mqLevel);
void utf8toAscii(char* dst, const char* src, int maxLen);
