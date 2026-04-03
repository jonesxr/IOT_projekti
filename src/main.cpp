/**
 * Älykäs Dashboard - ESP32-S3 + ILI9486 3.5" TFT
 * Refaktoroitu rakenne:
 *  - api/nysse.cpp (Bussi-rajapinta)
 *  - ui/homescreen.cpp (Käyttöliittymä)
 *  - sensors/bh1750_sensor.cpp (Valoanturi)
 */

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "ui/display.h"
#include "ui/homescreen.h"
#include "api/nysse.h"
#include "sensors/bh1750_sensor.h"
#include "sensors/inmp441_sensor.h"

// Alustetaan näyttö (Määritelty display.h:ssa)
LGFX gfx;

bool connectWiFi() {
  gfx.fillScreen(C_BG);
  // WiFi-ikoni (yksinkertainen)
  gfx.fillCircle(gfx.width()/2, 120, 60, C_HEADER);
  gfx.fillArc(gfx.width()/2, 120, 55, 45, 200, 340, C_ACCENT);
  gfx.fillArc(gfx.width()/2, 120, 40, 30, 210, 330, C_ACCENT);
  gfx.fillCircle(gfx.width()/2, 120, 8, C_ACCENT);

  drawWifiScreen("Yhdistetaan...", 0, C_YELLOW);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
    drawWifiScreen("Yhdistetaan...", attempts*3, C_YELLOW);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected.");
    drawWifiScreen("Yhdistetty!", 100, C_GREEN);
    delay(1200);
    return true;
  } else {
    drawWifiScreen("Yhteys epaonnistui!", 0, C_RED);
    delay(2000);
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && (millis()-t) < 5000) delay(10);
  Serial.println("\n=== Dashboard kaynnistyy ===");

  gfx.init();
  gfx.setRotation(0);
  initColors();
  gfx.fillScreen(C_BG);

  // Käynnistetään anturit
  initLightSensor();
  initMicrophone();

  // WiFi
  connectWiFi();

  // NTP aika (Suomi UTC+3)
  configTime(3*3600, 0, "pool.ntp.org", "time.google.com");

  Serial.println("Odotetaan verkkoa...");
  delay(1000);

  Serial.println("Haetaan Nysse-dataa...");
  fetchOk = fetchNysse();
  
  // Ensimmäinen piirto
  drawHomeScreen(getLightLevelLux());
}

void loop() {
  static unsigned long lastDraw = 0;
  static unsigned long lastVU = 0;
  
  // Päivitä VU-mittari 100ms välein (erittäin nopea päivitys)
  if (millis() - lastVU > 100 || lastVU == 0) {
    lastVU = millis();
    int vol = getMicrophoneVolume();
    updateVUMeter(vol);
  }

  // Päivitä koko näyttö 10 sekunnin välein
  if (millis() - lastDraw > 10000 || lastDraw == 0) {
    lastDraw = millis();
    
    // Hae uusi Nysse-data minuutin välein (60000ms)
    if (millis() - lastFetchMs > 60000 || lastFetchMs == 0) {
      lastFetchMs = millis();
      fetchOk = fetchNysse();
    }
    
    // Piirrä näyttö ja hae uusin valoisuus
    drawHomeScreen(getLightLevelLux());
  }
  
  delay(10); // Pieni viive virransäästöön
}